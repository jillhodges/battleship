/*
 * Arduino Mega — UART Communication with ESP32
 *
 * WHAT THIS CODE DOES:
 *   - Reads 4x4 switch grid (2x 74HC165 shift registers)
 *   - Reads 8 beam break sensors (1x 74HC165 shift register)
 *   - Sends both to ESP32 over UART as a structured packet
 *   - Receives joystick + button data from ESP32 and makes it
 *     available for use by other Mega code (LEDs, LCDs, logic)
 *
 * UART WIRING (via logic level shifter — required, Mega=5V, ESP32=3.3V):
 *   Mega TX1 (Pin 18) → Level Shifter HV → LV → ESP32 RX  (e.g. GPIO 16)
 *   Mega RX1 (Pin 19) → Level Shifter HV → LV → ESP32 TX  (e.g. GPIO 17)
 *   Mega GND          → Level Shifter GND + ESP32 GND (all tied together)
 *   Mega 5V           → Level Shifter HV
 *   ESP32 3.3V        → Level Shifter LV
 *
 * SHIFT REGISTER WIRING:
 *   Switch grid  (Chips 1+2): LOAD=Pin 8,  CLK=Pin 9,  DATA=Pin 11
 *   Beam breaks  (Chip 3):    LOAD=Pin 5,  CLK=Pin 6,  DATA=Pin 7
 *   Beam break sensors: active LOW (beam intact = HIGH, broken = LOW)
 *   Use 10k pull-up resistors on each beam break input pin.
 *
 * PACKET FORMAT (Mega → ESP32):
 *   START_BYTE | GRID_HIGH | GRID_LOW | BEAM_BYTE | CHECKSUM | END_BYTE
 *   6 bytes total
 *   GRID_HIGH + GRID_LOW = 16-bit switch grid (bit 15 = grid[0][0])
 *   BEAM_BYTE = 8 beam break states (bit 7 = sensor 0, 1=broken 0=clear)
 *   CHECKSUM  = XOR of GRID_HIGH, GRID_LOW, BEAM_BYTE
 *
 * PACKET FORMAT (ESP32 → Mega):
 *   START_BYTE | CTRL | LX1 | LY1 | LX2 | LY2 | BUTTONS1 | BUTTONS2 | CHECKSUM | END_BYTE
 *   10 bytes total
 *   CTRL      = bitmask of which controllers are connected (bit0=ctrl1, bit1=ctrl2)
 *   LX1,LY1   = controller 1 left stick axes, mapped 0–255 (127=centre)
 *   LX2,LY2   = controller 2 left stick axes, mapped 0–255 (127=centre)
 *   BUTTONS1  = controller 1 button bitmask (see below)
 *   BUTTONS2  = controller 2 button bitmask
 *   CHECKSUM  = XOR of all bytes between START and CHECKSUM
 *
 * BUTTON BITMASK (per controller):
 *   Bit 0 = Cross   Bit 1 = Circle   Bit 2 = Square   Bit 3 = Triangle
 *   Bit 4 = L1      Bit 5 = R1       Bit 6 = L2       Bit 7 = R2
 */

// ----- UART -----
#define SERIAL_ESP  Serial1       // Uses Mega pins 18 (TX) and 19 (RX)
#define BAUD_RATE   115200

// ----- Packet bytes -----
#define START_BYTE  0xAA
#define END_BYTE    0x55

// ----- Switch grid shift register pins -----
#define GRID_LOAD   8
#define GRID_CLK    9
#define GRID_DATA   11

// ----- Beam break shift register pins -----
#define BEAM_LOAD   5
#define BEAM_CLK    6
#define BEAM_DATA   7

// ----- Grid dimensions -----
#define ROWS  4
#define COLS  4
#define NUM_SENSORS 8

// ----- Grid and sensor state -----
bool grid[ROWS][COLS];
bool beamBroken[NUM_SENSORS];

// ----- Received controller state -----
struct ControllerState {
  bool connected;
  int  lx;          // -511 to +511
  int  ly;
  bool cross, circle, square, triangle;
  bool l1, r1, l2, r2;
};
ControllerState ctrl[2];

// ----- Timing -----
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 20;  // ms — send at ~50Hz max

// ============================================================
// SHIFT REGISTER READS
// ============================================================

uint16_t readGridShiftRegisters() {
  uint16_t data = 0;
  digitalWrite(GRID_LOAD, LOW);
  delayMicroseconds(5);
  digitalWrite(GRID_LOAD, HIGH);
  for (int i = 0; i < 16; i++) {
    data <<= 1;
    if (digitalRead(GRID_DATA) == HIGH) data |= 1;
    digitalWrite(GRID_CLK, HIGH);
    delayMicroseconds(5);
    digitalWrite(GRID_CLK, LOW);
    delayMicroseconds(5);
  }
  return data;
}

uint8_t readBeamShiftRegister() {
  uint8_t data = 0;
  digitalWrite(BEAM_LOAD, LOW);
  delayMicroseconds(5);
  digitalWrite(BEAM_LOAD, HIGH);
  for (int i = 0; i < 8; i++) {
    data <<= 1;
    if (digitalRead(BEAM_DATA) == HIGH) data |= 1;
    digitalWrite(BEAM_CLK, HIGH);
    delayMicroseconds(5);
    digitalWrite(BEAM_CLK, LOW);
    delayMicroseconds(5);
  }
  return data;
}

void updateGrid(uint16_t raw) {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      grid[r][c] = (raw >> (15 - (r * COLS + c))) & 0x01;
}

void updateBeams(uint8_t raw) {
  for (int i = 0; i < NUM_SENSORS; i++)
    // Beam break sensors are active LOW — LOW = beam broken
    beamBroken[i] = !((raw >> (7 - i)) & 0x01);
}

// ============================================================
// SEND PACKET TO ESP32
// ============================================================

void sendPacket(uint16_t gridRaw, uint8_t beamRaw) {
  uint8_t gridHigh = (gridRaw >> 8) & 0xFF;
  uint8_t gridLow  = gridRaw & 0xFF;
  uint8_t checksum = gridHigh ^ gridLow ^ beamRaw;

  SERIAL_ESP.write(START_BYTE);
  SERIAL_ESP.write(gridHigh);
  SERIAL_ESP.write(gridLow);
  SERIAL_ESP.write(beamRaw);
  SERIAL_ESP.write(checksum);
  SERIAL_ESP.write(END_BYTE);
}

// ============================================================
// RECEIVE PACKET FROM ESP32
// ============================================================

void receivePacket() {
  // Need at least 10 bytes for a full packet
  if (SERIAL_ESP.available() < 10) return;

  // Hunt for start byte
  if (SERIAL_ESP.peek() != START_BYTE) {
    SERIAL_ESP.read();  // Discard and try next byte
    return;
  }

  // Read full packet
  uint8_t buf[10];
  SERIAL_ESP.readBytes(buf, 10);

  // Validate start and end bytes
  if (buf[0] != START_BYTE || buf[9] != END_BYTE) return;

  // Validate checksum (XOR of bytes 1–7)
  uint8_t checksum = 0;
  for (int i = 1; i <= 7; i++) checksum ^= buf[i];
  if (checksum != buf[8]) {
    Serial.println("WARNING: Bad checksum on received packet, discarding.");
    return;
  }

  // Parse controller connection bitmask
  uint8_t connected = buf[1];
  ctrl[0].connected = connected & 0x01;
  ctrl[1].connected = connected & 0x02;

  // Parse axes — convert 0–255 back to -511 to +511
  ctrl[0].lx = map(buf[2], 0, 255, -511, 511);
  ctrl[0].ly = map(buf[3], 0, 255, -511, 511);
  ctrl[1].lx = map(buf[4], 0, 255, -511, 511);
  ctrl[1].ly = map(buf[5], 0, 255, -511, 511);

  // Parse button bitmasks
  for (int c = 0; c < 2; c++) {
    uint8_t b = buf[6 + c];
    ctrl[c].cross    = b & 0x01;
    ctrl[c].circle   = b & 0x02;
    ctrl[c].square   = b & 0x04;
    ctrl[c].triangle = b & 0x08;
    ctrl[c].l1       = b & 0x10;
    ctrl[c].r1       = b & 0x20;
    ctrl[c].l2       = b & 0x40;
    ctrl[c].r2       = b & 0x80;
  }
}

// ============================================================
// SETUP & LOOP
// ============================================================

void setup() {
  Serial.begin(115200);      // Debug monitor
  SERIAL_ESP.begin(BAUD_RATE);

  // Grid shift register pins
  pinMode(GRID_LOAD, OUTPUT); pinMode(GRID_CLK, OUTPUT); pinMode(GRID_DATA, INPUT);
  digitalWrite(GRID_LOAD, HIGH); digitalWrite(GRID_CLK, LOW);

  // Beam shift register pins
  pinMode(BEAM_LOAD, OUTPUT); pinMode(BEAM_CLK, OUTPUT); pinMode(BEAM_DATA, INPUT);
  digitalWrite(BEAM_LOAD, HIGH); digitalWrite(BEAM_CLK, LOW);

  Serial.println("Mega UART communication ready.");
}

void loop() {
  // --- Always try to receive from ESP32 ---
  receivePacket();

  // --- Send at ~50Hz max ---
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = millis();

    uint16_t gridRaw = readGridShiftRegisters();
    uint8_t  beamRaw = readBeamShiftRegister();

    updateGrid(gridRaw);
    updateBeams(beamRaw);
    sendPacket(gridRaw, beamRaw);

    // --- Debug output ---
Serial.println("--- Mega State ---");
for (int r = 0; r < ROWS; r++) {
  for (int c = 0; c < COLS; c++)
    Serial.print(grid[r][c] ? "[X]" : "[ ]");
  Serial.println();
}

for (int i = 0; i < NUM_SENSORS; i++) {
  Serial.print("Beam ");
  Serial.print(i);
  Serial.print(": ");
  Serial.println(beamBroken[i] ? "BROKEN" : "clear");
}

for (int c = 0; c < 2; c++) {
  if (ctrl[c].connected) {
    Serial.print("Ctrl ");
    Serial.print(c + 1);
    Serial.print(": LX="); Serial.print(ctrl[c].lx);
    Serial.print(" LY="); Serial.print(ctrl[c].ly);
    Serial.print(" | Cross="); Serial.print(ctrl[c].cross);
    Serial.print(" Circle="); Serial.print(ctrl[c].circle);
    Serial.print(" L1="); Serial.print(ctrl[c].l1);
    Serial.print(" R1="); Serial.println(ctrl[c].r1);
  }
}
Serial.println();
  }

  // Your LED strip, LCD, and game logic code goes here —
  // grid[][], beamBroken[], and ctrl[] are always up to date.
}
