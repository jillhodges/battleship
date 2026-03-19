/*
 * PS4 Dual Controller Bluetooth Troubleshooter
 * ESP32 + Bluepad32
 *
 * No servos — just prints all controller data to Serial Monitor
 * so you can verify both controllers connect and send correct values.
 *
 * SETUP:
 *   Install Bluepad32 board package (see URLs below in File → Preferences):
 *     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
 *     https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json
 *
 * PAIRING:
 *   Hold PS button + Share simultaneously until light bar flashes rapidly.
 *   Do one controller at a time — wait for "Controller connected" before pairing the second.
 *
 * WHAT TO CHECK:
 *   1. Both controllers show "Controller connected, assigned index 0/1"
 *   2. Joystick axes read near 0 at rest, and sweep -511 to +511 when moved
 *   3. Button presses register correctly
 *   4. No unexpected disconnections
 */

#include <Bluepad32.h>

ControllerPtr controllers[BP32_MAX_GAMEPADS];

// ----- Connected callback -----
void onConnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (controllers[i] == nullptr) {
      controllers[i] = ctl;

      if (i == 0) ctl->setColorLED(0, 0, 255);   // Blue
      if (i == 1) ctl->setColorLED(255, 0, 0);   // Red

      Serial.println("===========================================");
      Serial.printf("  Controller %d connected!\n", i + 1);
      Serial.printf("  Light bar: %s\n", i == 0 ? "BLUE" : "RED");
      Serial.println("  Move sticks and press buttons to test.");
      Serial.println("===========================================");
      return;
    }
  }
  Serial.println("WARNING: Controller connected but no slot free (max 2).");
}

// ----- Disconnected callback -----
void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (controllers[i] == ctl) {
      controllers[i] = nullptr;
      Serial.println("===========================================");
      Serial.printf("  Controller %d DISCONNECTED.\n", i + 1);
      Serial.println("===========================================");
      return;
    }
  }
}

// ----- Print all useful data for one controller -----
void printController(int index) {
  ControllerPtr ctl = controllers[index];
  if (ctl == nullptr || !ctl->isConnected()) return;

  Serial.printf("--- Controller %d ---\n", index + 1);

  // Joystick axes (-511 to +511, centre = ~0)
  Serial.printf("  Left  stick: X=%4d  Y=%4d\n", ctl->axisX(), ctl->axisY());
  Serial.printf("  Right stick: X=%4d  Y=%4d\n", ctl->axisRX(), ctl->axisRY());

  // Triggers (0 to 1023)
  Serial.printf("  L2=%4d  R2=%4d\n", ctl->brake(), ctl->throttle());

  // Buttons (print only pressed ones to keep output readable)
  String pressed = "";
  if (ctl->a())          pressed += "Cross ";
  if (ctl->b())          pressed += "Circle ";
  if (ctl->x())          pressed += "Square ";
  if (ctl->y())          pressed += "Triangle ";
  if (ctl->l1())         pressed += "L1 ";
  if (ctl->r1())         pressed += "R1 ";
  if (ctl->l2())         pressed += "L2 ";
  if (ctl->r2())         pressed += "R2 ";
  if (ctl->thumbL())     pressed += "L3 ";
  if (ctl->thumbR())     pressed += "R3 ";
  if (ctl->miscHome())   pressed += "PS ";
  if (ctl->miscSelect()) pressed += "Share ";
  if (ctl->miscStart())  pressed += "Options ";

  if (pressed.length() > 0) {
    Serial.printf("  Buttons: %s\n", pressed.c_str());
  }

  // Battery level
  Serial.printf("  Battery: %d%%\n", ctl->battery());

  Serial.println();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("PS4 Bluetooth Troubleshooter — Waiting for controllers...");
  Serial.println("Hold PS + Share on each controller to pair.");
  Serial.println();

  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) controllers[i] = nullptr;

  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys();  // Force fresh pairing each boot
}

void loop() {
  bool dataUpdated = BP32.update();

  if (dataUpdated) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
      printController(i);
    }
  }

  vTaskDelay(1);
}
