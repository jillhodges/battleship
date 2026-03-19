/*
 * D645MW Servo Troubleshooter — Serial Input Control
 * ESP32
 *
 * No controller — type commands into Serial Monitor to move servos
 * and verify they respond correctly across their full range.
 *
 * WIRING:
 *   Servo 1 signal → GPIO 13
 *   Servo 2 signal → GPIO 14
 *   Servo 3 signal → GPIO 25
 *   Servo 4 signal → GPIO 26
 *   Servo power    → External 6V–7.4V supply (NOT ESP32 pins)
 *   Shared GND     → External supply GND + ESP32 GND tied together
 *
 * COMMANDS (type into Serial Monitor, set line ending to "Newline"):
 *   all <degrees>        — move all servos to same position  e.g. "all 90"
 *   s1 <degrees>         — move servo 1 only                 e.g. "s1 45"
 *   s2 <degrees>         — move servo 2 only                 e.g. "s2 180"
 *   s3 <degrees>         — move servo 3 only                 e.g. "s3 0"
 *   s4 <degrees>         — move servo 4 only                 e.g. "s4 135"
 *   centre               — move all servos to 90°
 *   sweep                — run all servos through full range (0→180→0)
 *   status               — print current position of all servos
 *
 * Degrees must be 0–180. Values outside this range are clamped.
 */

#include <ESP32Servo.h>

// ----- Servo pins -----
#define SERVO1_PIN  13
#define SERVO2_PIN  14
#define SERVO3_PIN  25
#define SERVO4_PIN  26

// ----- D645MW pulse width limits (µs) -----
#define SERVO_MIN_US  900
#define SERVO_MAX_US  2100

Servo servo1, servo2, servo3, servo4;

// Track current positions for status command
int pos[4] = {90, 90, 90, 90};

// ----- Move a servo by index (0–3) to a degree position -----
void moveServo(int index, int degrees) {
  degrees = constrain(degrees, 0, 180);
  int pulse = map(degrees, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
  pos[index] = degrees;

  switch (index) {
    case 0: servo1.writeMicroseconds(pulse); break;
    case 1: servo2.writeMicroseconds(pulse); break;
    case 2: servo3.writeMicroseconds(pulse); break;
    case 3: servo4.writeMicroseconds(pulse); break;
  }

  Serial.printf("  Servo %d → %d° (%dµs)\n", index + 1, degrees, pulse);
}

// ----- Sweep all servos 0→180→0 -----
void sweep() {
  Serial.println("  Sweeping 0° → 180°...");
  for (int deg = 0; deg <= 180; deg += 5) {
    for (int i = 0; i < 4; i++) moveServo(i, deg);
    delay(30);
  }
  Serial.println("  Sweeping 180° → 0°...");
  for (int deg = 180; deg >= 0; deg -= 5) {
    for (int i = 0; i < 4; i++) moveServo(i, deg);
    delay(30);
  }
  Serial.println("  Sweep complete.");
}

// ----- Print current positions -----
void printStatus() {
  Serial.println("  Current positions:");
  for (int i = 0; i < 4; i++) {
    int pulse = map(pos[i], 0, 180, SERVO_MIN_US, SERVO_MAX_US);
    Serial.printf("    Servo %d: %d° (%dµs)\n", i + 1, pos[i], pulse);
  }
}

// ----- Print help -----
void printHelp() {
  Serial.println("Commands:");
  Serial.println("  all <deg>   — all servos to same angle    e.g. all 90");
  Serial.println("  s1 <deg>    — servo 1 only                e.g. s1 45");
  Serial.println("  s2 <deg>    — servo 2 only                e.g. s2 180");
  Serial.println("  s3 <deg>    — servo 3 only                e.g. s3 0");
  Serial.println("  s4 <deg>    — servo 4 only                e.g. s4 135");
  Serial.println("  centre      — all servos to 90°");
  Serial.println("  sweep       — full range test (0→180→0)");
  Serial.println("  status      — show current positions");
  Serial.println("  help        — show this list");
  Serial.println();
}

// ----- Parse and execute Serial command -----
void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  Serial.printf("> %s\n", cmd.c_str());

  if (cmd == "centre") {
    Serial.println("  Centring all servos to 90°");
    for (int i = 0; i < 4; i++) moveServo(i, 90);

  } else if (cmd == "sweep") {
    sweep();

  } else if (cmd == "status") {
    printStatus();

  } else if (cmd == "help") {
    printHelp();

  } else if (cmd.startsWith("all ")) {
    int deg = cmd.substring(4).toInt();
    Serial.printf("  Moving all servos to %d°\n", deg);
    for (int i = 0; i < 4; i++) moveServo(i, deg);

  } else if (cmd.startsWith("s1 ")) {
    moveServo(0, cmd.substring(3).toInt());

  } else if (cmd.startsWith("s2 ")) {
    moveServo(1, cmd.substring(3).toInt());

  } else if (cmd.startsWith("s3 ")) {
    moveServo(2, cmd.substring(3).toInt());

  } else if (cmd.startsWith("s4 ")) {
    moveServo(3, cmd.substring(3).toInt());

  } else {
    Serial.println("  Unknown command. Type 'help' for list of commands.");
  }

  Serial.println();
}

// ----- Setup -----
void setup() {
  Serial.begin(115200);

  servo1.attach(SERVO1_PIN, SERVO_MIN_US, SERVO_MAX_US);
  servo2.attach(SERVO2_PIN, SERVO_MIN_US, SERVO_MAX_US);
  servo3.attach(SERVO3_PIN, SERVO_MIN_US, SERVO_MAX_US);
  servo4.attach(SERVO4_PIN, SERVO_MIN_US, SERVO_MAX_US);

  // Centre all on boot
  for (int i = 0; i < 4; i++) moveServo(i, 90);

  Serial.println();
  Serial.println("D645MW Servo Troubleshooter Ready.");
  Serial.println("Make sure Serial Monitor line ending is set to 'Newline'.");
  Serial.println();
  printHelp();
}

// ----- Main loop -----
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    handleCommand(cmd);
  }
}
