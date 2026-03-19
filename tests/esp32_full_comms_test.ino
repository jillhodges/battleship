/*
 * ESP32 — UART Communication with Arduino Mega
 *
 * WHAT THIS CODE DOES:
 *   - Receives 4x4 switch grid + beam break states from Mega
 *   - Reads two PS4 controllers via Bluepad32
 *   - Drives 4 servos from joystick axes
 *   - Sends controller state to Mega over UART
 *
 * UART WIRING (via logic level shifter — required, ESP32=3.3V, Mega=5V):
 *   ESP32 TX (GPIO 17) → Level Shifter LV → HV → Mega RX1 (Pin 19)
 *   ESP32 RX (GPIO 16) → Level Shifter LV → HV → Mega TX1 (Pin 18)
 *   ESP32 GND          → Level Shifter GND + Mega GND (all tied together)
 *   ESP32 3.3V         → Level Shifter LV
 *   Mega 5V            → Level Shifter HV
 *
 * SERVO PINS:
 *   Servo 1 → GPIO 13   (Controller 1, left stick X)
 *   Servo 2 → GPIO 14   (Controller 1, left stick Y)
 *   Servo 3 → GPIO 25   (Controller 2, left stick X)
 *   Servo 4 → GPIO 26   (Controller 2, left stick Y)
 *
 * PACKET FORMAT (Mega → ESP32):
 *   START_BYTE | GRID_HIGH | GRID_LOW | BEAM_BYTE | CHECKSUM | END_BYTE
 *
 * PACKET FORMAT (ESP32 → Mega):
 *   START_BYTE | CTRL | LX1 | LY1 | LX2 | LY2 | BUTTONS1 | BUTTONS2 | CHECKSUM | END_BYTE
 *
 * BUTTON BITMASK (per controller):
 *   Bit 0=Cross  Bit 1=Circle  Bit 2=Square  Bit 3=Triangle
 *   Bit 4=L1     Bit 5=R1      Bit 6=L2      Bit 7=R2
 */

#include <Bluepad32.h>
#include <ESP32Servo.h>

// ----- UART -----
#define SERIAL_MEGA  Serial2      // ESP32 Serial2: TX=GPIO17, RX=GPIO16
#define BAUD_RATE    115200

// ----- Packet bytes -----
#define START_BYTE   0xAA
#define END_BYTE     0x55

// ----- Servo pins + limits -----
#define SERVO1_PIN   13
#define SERVO2_PIN   14
#define SERVO3_PIN   25
#define SERVO4_PIN   26
#define SERVO_MIN_US 900
#define SERVO_MAX_US 2100
#define DEADZONE     15

Servo servo1, servo2, servo3, servo4;

// ----- Controllers -----
ControllerPtr controllers[BP32_MAX_GAMEPADS];

// ----- Received state from Mega -----
uint16_t gridState = 0;    // 16-bit switch grid
uint8_t  beamState = 0;    // 8 beam break sensors

// ----- Timing -----
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 20;  // ms

// ============================================================
// BLUEPAD32 CALLBACKS
// ============================================================

void onConnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (controllers[i] == nullptr) {
      controllers[i] = ctl;
      if (i == 0) ctl->setColorLED(0, 0, 255);
      if (i == 1) ctl->setColorLED(255, 0, 0);
      Serial.printf("Controller %d connected.\n", i + 1);
      return;
    }
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (controllers[i] == ctl) {
      controllers[i] = nullptr;
      if (i == 0) { servo1.writeMicroseconds(1500); servo2.writeMicroseconds(1500); }
      if (i == 1) { servo3.writeMicroseconds(1500); servo4.writeMicroseconds(1500); }
      Serial.printf("Controller %d disconnected.\n", i + 1);
      return;
    }
  }
}

// ============================================================
// SERVO HELPERS
// ============================================================

int axisToPulse(int axis) {
  if (abs(axis) < DEADZONE) return 1500;
  return map(axis, -511, 511, SERVO_MIN_US, SERVO_MAX_US);
}

void updateServos() {
  if (controllers[0] != nullptr && controllers[0]->isConnected()) {
    servo1.writeMicroseconds(axisToPulse(controllers[0]->axisX()));
    servo2.writeMicroseconds(axisToPulse(controllers[0]->axisY()));
  }
  if (controllers[1] != nullptr && controllers[1]->isConnected()) {
    servo3.writeMicroseconds(axisToPulse(controllers[1]->axisX()));
    servo4.writeMicroseconds(axisToPulse(controllers[1]->axisY()));
  }
}

// ============================================================
// SEND PACKET TO MEGA
// ============================================================

uint8_t buildButtonByte(ControllerPtr ctl) {
  if (ctl == nullptr || !ctl->isConnected()) return 0;
  uint8_t b = 0;
  if (ctl->a())    b |= 0x01;  // Cross
  if (ctl->b())    b |= 0x02;  // Circle
  if (ctl->x())    b |= 0x04;  // Square
  if (ctl->y())    b |= 0x08;  // Triangle
  if (ctl->l1())   b |= 0x10;
  if (ctl->r1())   b |= 0x20;
  if (ctl->l2())   b |= 0x40;
  if (ctl->r2())   b |= 0x80;
  return b;
}

void sendPacket() {
  // Controller connection bitmask
  uint8_t connected = 0;
  if (controllers[0] != nullptr && controllers[0]->isConnected()) connected |= 0x01;
  if (controllers[1] != nullptr && controllers[1]->isConnected()) connected |= 0x02;

  // Map axes -511..511 → 0..255 to fit in one byte
  auto axisToByteVal = [](ControllerPtr ctl, bool useX) -> uint8_t {
    if (ctl == nullptr || !ctl->isConnected()) return 127;
    int val = useX ? ctl->axisX() : ctl->axisY();
    return (uint8_t)map(val, -511, 511, 0, 255);
  };

  uint8_t lx1 = axisToByteVal(controllers[0], true);
  uint8_t ly1 = axisToByteVal(controllers[0], false);
  uint8_t lx2 = axisToByteVal(controllers[1], true);
  uint8_t ly2 = axisToByteVal(controllers[1], false);
  uint8_t btn1 = buildButtonByte(controllers[0]);
  uint8_t btn2 = buildButtonByte(controllers[1]);

  uint8_t checksum = connected ^ lx1 ^ ly1 ^ lx2 ^ ly2 ^ btn1 ^ btn2;

  SERIAL_MEGA.write(START_BYTE);
  SERIAL_MEGA.write(connected);
  SERIAL_MEGA.write(lx1);
  SERIAL_MEGA.write(ly1);
  SERIAL_MEGA.write(lx2);
  SERIAL_MEGA.write(ly2);
  SERIAL_MEGA.write(btn1);
  SERIAL_MEGA.write(btn2);
  SERIAL_MEGA.write(checksum);
  SERIAL_MEGA.write(END_BYTE);
}

// ============================================================
// RECEIVE PACKET FROM MEGA
// ============================================================

void receivePacket() {
  if (SERIAL_MEGA.available() < 6) return;

  if (SERIAL_MEGA.peek() != START_BYTE) {
    SERIAL_MEGA.read();
    return;
  }

  uint8_t buf[6];
  SERIAL_MEGA.readBytes(buf, 6);

  if (buf[0] != START_BYTE || buf[5] != END_BYTE) return;

  uint8_t checksum = buf[1] ^ buf[2] ^ buf[3];
  if (checksum != buf[4]) {
    Serial.println("WARNING: Bad checksum on received packet, discarding.");
    return;
  }

  gridState = ((uint16_t)buf[1] << 8) | buf[2];
  beamState = buf[3];
}

// ============================================================
// HELPERS — read parsed state elsewhere in your code
// ============================================================

// Returns true if switch at row r, col c is occupied
bool gridOccupied(int r, int c) {
  return (gridState >> (15 - (r * 4 + c))) & 0x01;
}

// Returns true if beam sensor i is broken (0–7)
bool beamIsBroken(int i) {
  return (beamState >> (7 - i)) & 0x01;
}

// ============================================================
// SETUP & LOOP
// ============================================================

void setup() {
  Serial.begin(115200);
  SERIAL_MEGA.begin(BAUD_RATE, SERIAL_8N1, 16, 17);  // RX=GPIO16, TX=GPIO17

  servo1.attach(SERVO1_PIN, SERVO_MIN_US, SERVO_MAX_US);
  servo2.attach(SERVO2_PIN, SERVO_MIN_US, SERVO_MAX_US);
  servo3.attach(SERVO3_PIN, SERVO_MIN_US, SERVO_MAX_US);
  servo4.attach(SERVO4_PIN, SERVO_MIN_US, SERVO_MAX_US);

  servo1.writeMicroseconds(1500);
  servo2.writeMicroseconds(1500);
  servo3.writeMicroseconds(1500);
  servo4.writeMicroseconds(1500);

  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) controllers[i] = nullptr;
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys();

  Serial.println("ESP32 UART communication ready.");
  Serial.println("Hold PS + Share on each controller to pair.");
}

void loop() {
  // --- Update Bluepad32 ---
  bool dataUpdated = BP32.update();

  // --- Always try to receive from Mega ---
  receivePacket();

  if (dataUpdated) {
    updateServos();
  }

  // --- Send to Mega at ~50Hz ---
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = millis();
    sendPacket();

    // --- Debug output ---
    Serial.printf("Grid: 0x%04X | Beams: 0x%02X\n", gridState, beamState);
    for (int i = 0; i < 2; i++) {
      if (controllers[i] != nullptr && controllers[i]->isConnected()) {
        Serial.printf("Ctrl %d: LX=%d LY=%d\n",
          i+1, controllers[i]->axisX(), controllers[i]->axisY());
      }
    }
  }

  vTaskDelay(1);
}

