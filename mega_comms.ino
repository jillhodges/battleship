/*
 * mega_comms_test.ino
 * Fixed version — single ControllerData ctrl instance,
 * no stray braces, no byte-consuming debug in loop.
 */

// ----- UART -----
#define SERIAL_ESP  Serial1
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
#define ROWS        4
#define COLS        4
#define NUM_SENSORS 8

// ----- Grid and sensor state -----
bool grid[ROWS][COLS];
bool beamBroken[NUM_SENSORS];

// ----- Received controller state -----
struct ControllerData {
  bool connected;
  int  lx;
  int  ly;
  bool cross, circle, square, triangle;
  bool l1, r1, l2, r2;
};
ControllerData ctrl;

// ----- Timing -----
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 20;

// ============================================================
// SHIFT REGISTER READS
// ============================================================

uint16_t readGridShiftRegisters() {
  uint16_t data = 0;
  digitalWrite(GRID_LOAD, LOW);
  delayMicroseconds(5);
  digitalWrite(GRID_LOAD, HIGH);
  delayMicroseconds(5);
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
    beamBroken[i] = !((raw >> (7 - i)) & 0x01);
}

// ============================================================
// SEND PACKET TO ESP32
// ============================================================

void sendPacket(uint16_t gridRaw, uint8_t beamRaw) {
  uint8_t gridHigh = (gridRaw >> 8) & 0xFF;
  uint8_t gridLow  =  gridRaw       & 0xFF;
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
  if (SERIAL_ESP.available() < 10) return;

  if (SERIAL_ESP.peek() != START_BYTE) {
    SERIAL_ESP.read();
    return;
  }

  unsigned long start = millis();
  while (SERIAL_ESP.available() < 10) {
    if (millis() - start > 100) return;
  }

  uint8_t buf[10];
  for (int i = 0; i < 10; i++) {
    buf[i] = SERIAL_ESP.read();
  }

  if (buf[0] != START_BYTE || buf[9] != END_BYTE) {
    Serial.println("Bad start/end bytes.");
    return;
  }

  uint8_t checksum = 0;
  for (int i = 1; i <= 7; i++) checksum ^= buf[i];
  if (checksum != buf[8]) {
    Serial.print("Bad checksum. Got: 0x");
    Serial.print(buf[8], HEX);
    Serial.print(" Expected: 0x");
    Serial.println(checksum, HEX);
    return;
  }

  ctrl.connected = buf[1] & 0x01;
ctrl.lx        = map(buf[2], 0, 255, -511, 511);
ctrl.ly        = map(buf[3], 0, 255, -511, 511);

uint8_t b     = buf[6];   // changed from buf[4] to buf[6]
ctrl.cross    = b & 0x01;
ctrl.circle   = b & 0x02;
ctrl.square   = b & 0x04;
ctrl.triangle = b & 0x08;
ctrl.l1       = b & 0x10;
ctrl.r1       = b & 0x20;
}
// ============================================================
// SETUP & LOOP
// ============================================================

void setup() {
  Serial.begin(115200);
  SERIAL_ESP.begin(BAUD_RATE);

  pinMode(GRID_LOAD, OUTPUT); pinMode(GRID_CLK, OUTPUT); pinMode(GRID_DATA, INPUT);
  digitalWrite(GRID_LOAD, HIGH); digitalWrite(GRID_CLK, LOW);

  pinMode(BEAM_LOAD, OUTPUT); pinMode(BEAM_CLK, OUTPUT); pinMode(BEAM_DATA, INPUT);
  digitalWrite(BEAM_LOAD, HIGH); digitalWrite(BEAM_CLK, LOW);

  ctrl.connected = false;

  Serial.println("Mega UART communication ready.");
}

void loop() {
  // Always try to receive from ESP32
  receivePacket();

  // Send and print at ~50Hz
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = millis();

    uint16_t gridRaw = readGridShiftRegisters();
    uint8_t  beamRaw = readBeamShiftRegister();

    updateGrid(gridRaw);
    updateBeams(beamRaw);
    sendPacket(gridRaw, beamRaw);

    // --- Grid debug ---
    Serial.println("--- Switch Grid ---");
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++)
        Serial.print(grid[r][c] ? "[X]" : "[ ]");
      Serial.println();
    }

    // --- Controller debug ---
    if (ctrl.connected) {
      Serial.print("Ctrl: LX="); Serial.print(ctrl.lx);
      Serial.print(" LY=");      Serial.print(ctrl.ly);
      Serial.print(" | Cross="); Serial.print(ctrl.cross);
      Serial.print(" Circle=");  Serial.print(ctrl.circle);
      Serial.print(" L1=");      Serial.print(ctrl.l1);
      Serial.print(" R1=");      Serial.println(ctrl.r1);
    } else {
      Serial.println("Ctrl: not connected");
    }
    Serial.println();
  }
}
