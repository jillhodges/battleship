/*
 * Dual PS4 Controller → 4x D645MW Servo Control
 * ESP32 + Bluepad32
 *
 * Controller 1: Left stick X → Servo 1, Left stick Y → Servo 2
 * Controller 2: Left stick X → Servo 3, Left stick Y → Servo 4
 *
 * LIBRARY SETUP (Arduino IDE):
 *   Go to File → Preferences → Additional Board Manager URLs, add BOTH:
 *     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
 *     https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json
 *
 *   Then: Tools → Board → Board Manager → install "ESP32 + Bluepad32"
 *   Select your board under: Tools → Board → ESP32 + Bluepad32 Arduino
 *
 * PAIRING PS4 CONTROLLERS:
 *   Hold PS button + Share button simultaneously until the light bar
 *   flashes rapidly. The ESP32 will accept up to 2 controllers.
 *   Controllers are assigned index 0 and 1 in the order they connect.
 *
 * WIRING:
 *   Servo 1 signal → GPIO 13
 *   Servo 2 signal → GPIO 14
 *   Servo 3 signal → GPIO 25
 *   Servo 4 signal → GPIO 26
 *   Servo power (red)  → External 6V–7.4V supply (NOT ESP32 3.3V/5V)
 *   Servo ground       → External supply GND + ESP32 GND (tie together)
 *
 * IMPORTANT — Power:
 *   The ESP32 runs on 3.3V logic. Do NOT connect servo power to ESP32
 *   pins. Use an external supply and share the ground.
 *
 * D645MW PULSE WIDTH:
 *   900µs = 0°, 1500µs = 90° (centre), 2100µs = 180°
 *
 * PS4 JOYSTICK VALUES (Bluepad32):
 *   Range: -511 to +511
 *   Centre rests near 0 (may drift ±10, hence the deadzone below)
 */

#include <Bluepad32.h>
#include <ESP32Servo.h>

// ----- Servo pins -----
#define SERVO1_PIN  13    // Controller 1, left stick X
#define SERVO2_PIN  14    // Controller 1, left stick Y
#define SERVO3_PIN  25    // Controller 2, left stick X
#define SERVO4_PIN  26    // Controller 2, left stick Y

// ----- D645MW pulse width limits (µs) -----
#define SERVO_MIN_US  900
#define SERVO_MAX_US  2100

// ----- Joystick deadzone (ignore small drift near centre) -----
#define DEADZONE  15

// ----- Bluepad32 joystick axis range -----
#define JOY_MIN  -511
#define JOY_MAX   511

// ----- Servo objects -----
Servo servo1, servo2, servo3, servo4;

// ----- Controller pointers (Bluepad32 manages up to 4) -----
ControllerPtr controllers[BP32_MAX_GAMEPADS];

// ----- Helper: apply deadzone and map joystick axis to pulse width -----
int axisToPulse(int axisValue) {
  if (abs(axisValue) < DEADZONE) {
    return 1500;   // Centre position
  }
  return map(axisValue, JOY_MIN, JOY_MAX, SERVO_MIN_US, SERVO_MAX_US);
}

// ----- Bluepad32 callback: controller connected -----
void onConnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (controllers[i] == nullptr) {
      Serial.printf("Controller connected, assigned index %d\n", i);
      controllers[i] = ctl;

      // Set light bar colour to distinguish controllers
      if (i == 0) ctl->setColorLED(0, 0, 255);    // Blue for controller 1
      if (i == 1) ctl->setColorLED(255, 0, 0);    // Red for controller 2
      return;
    }
  }
  Serial.println("Controller connected but no slot available (max 2).");
}

// ----- Bluepad32 callback: controller disconnected -----
void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (controllers[i] == ctl) {
      Serial.printf("Controller %d disconnected.\n", i);
      controllers[i] = nullptr;

      // Centre servos for that controller on disconnect
      if (i == 0) { servo1.writeMicroseconds(1500); servo2.writeMicroseconds(1500); }
      if (i == 1) { servo3.writeMicroseconds(1500); servo4.writeMicroseconds(1500); }
      return;
    }
  }
}

// ----- Process inputs for a given controller index -----
void processController(int index) {
  ControllerPtr ctl = controllers[index];
  if (ctl == nullptr || !ctl->isConnected()) return;

  int axisLX = ctl->axisX();   // Left stick horizontal
  int axisLY = ctl->axisY();   // Left stick vertical

  int pulse1 = axisToPulse(axisLX);
  int pulse2 = axisToPulse(axisLY);

  if (index == 0) {
    servo1.writeMicroseconds(pulse1);
    servo2.writeMicroseconds(pulse2);

    Serial.printf("[Ctrl 1] LX=%4d → S1=%4dus | LY=%4d → S2=%4dus\n",
                  axisLX, pulse1, axisLY, pulse2);
  }

  if (index == 1) {
    servo3.writeMicroseconds(pulse1);
    servo4.writeMicroseconds(pulse2);

    Serial.printf("[Ctrl 2] LX=%4d → S3=%4dus | LY=%4d → S4=%4dus\n",
                  axisLX, pulse1, axisLY, pulse2);
  }
}

// ----- Setup -----
void setup() {
  Serial.begin(115200);

  // Attach servos with D645MW safe pulse range
  servo1.attach(SERVO1_PIN, SERVO_MIN_US, SERVO_MAX_US);
  servo2.attach(SERVO2_PIN, SERVO_MIN_US, SERVO_MAX_US);
  servo3.attach(SERVO3_PIN, SERVO_MIN_US, SERVO_MAX_US);
  servo4.attach(SERVO4_PIN, SERVO_MIN_US, SERVO_MAX_US);

  // Centre all servos on boot
  servo1.writeMicroseconds(1500);
  servo2.writeMicroseconds(1500);
  servo3.writeMicroseconds(1500);
  servo4.writeMicroseconds(1500);

  // Initialise controller array
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) controllers[i] = nullptr;

  // Initialise Bluepad32
  BP32.setup(&onConnectedController, &onDisconnectedController);

  // Optional: forget previously paired controllers so fresh pairing
  // is required each boot. Comment this out to auto-reconnect.
  BP32.forgetBluetoothKeys();

  Serial.println("Ready. Hold PS + Share on each controller to pair.");
  Serial.println("Controller 1 → Servos 1 & 2 | Controller 2 → Servos 3 & 4");
}

// ----- Main loop -----
void loop() {
  // BP32.update() fetches latest controller state — must be called each loop
  bool dataUpdated = BP32.update();

  if (dataUpdated) {
    processController(0);
    processController(1);
  }

  // Small yield to keep Bluetooth stack happy — do not use delay() here
  vTaskDelay(1);
}
