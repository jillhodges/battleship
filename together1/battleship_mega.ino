/*
 * battleship_mega.ino
 * Two-player arcade battleship game — Arduino Mega main sketch.
 *
 * LIBRARIES REQUIRED (install via Arduino Library Manager):
 *   - Adafruit NeoPixel
 *   - LiquidCrystal I2C (by Frank de Brabander)
 *
 * FILE STRUCTURE (all files in same sketch folder):
 *   battleship_mega.ino  — this file, setup() and loop()
 *   game_state.h         — shared structs, constants, global declarations
 *   game_logic.h/.cpp    — ship validation, beam break reading, shot resolution
 *   setup_phase.h/.cpp   — ship placement phase
 *   turn_phase.h/.cpp    — turn timer, shot detection, result handling
 *   display.h/.cpp       — all LCD output
 *   leds.h/.cpp          — all NeoPixel LED output
 *
 * WIRING SUMMARY:
 *   See individual pin definitions in game_state.h
 *   UART to ESP32 via logic level shifter: Mega TX1(18)/RX1(19)
 *   Player 1 LCD: I2C address 0x27
 *   Player 2 LCD: I2C address 0x26
 *   4x NeoPixel strips on pins 22, 23, 24, 25
 *   Shift registers: see P1_GRID_*, P2_GRID_*, BEAM_* pin defines
 *
 * SHIPS PER PLAYER:
 *   Ship 0 — Cruiser    (3 cells)
 *   Ship 1 — Destroyer  (2 cells)
 *   Ship 2 — Submarine  (2 cells)
 *   Ship 3 — Scout      (1 cell)
 */

#include "game_state.h"
#include "game_logic.h"
#include "setup_phase.h"
#include "turn_phase.h"
#include "display.h"
#include "leds.h"

// ============================================================
// GLOBAL GAME STATE DEFINITIONS
// (declared extern in game_state.h, defined here)
// ============================================================

GamePhase       gamePhase           = PHASE_SETUP;
Player          players[NUM_PLAYERS];
ControllerState ctrl[NUM_PLAYERS];
int             currentPlayer       = 0;
unsigned long   turnStartTime       = 0;
unsigned long   aimStopTime         = 0;
bool            playerStoppedAiming = false;
ShotResult      lastShotResult      = RESULT_NONE;
int             lastShotRow         = -1;
int             lastShotCol         = -1;
int             lastShotShipIndex   = -1;

// ============================================================
// UART — RECEIVE FROM ESP32
// ============================================================

#define START_BYTE  0xAA
#define END_BYTE    0x55

void receiveFromESP32() {
  if (SERIAL_ESP.available() < 10) return;
  if (SERIAL_ESP.peek() != START_BYTE) { SERIAL_ESP.read(); return; }

  uint8_t buf[10];
  SERIAL_ESP.readBytes(buf, 10);

  if (buf[0] != START_BYTE || buf[9] != END_BYTE) return;

  uint8_t checksum = 0;
  for (int i = 1; i <= 7; i++) checksum ^= buf[i];
  if (checksum != buf[8]) return;

  uint8_t connected = buf[1];
  ctrl[0].connected = connected & 0x01;
  ctrl[1].connected = (connected & 0x02) != 0;

  // Map 0-255 back to -511 to +511
  ctrl[0].lx = map(buf[2], 0, 255, -511, 511);
  ctrl[0].ly = map(buf[3], 0, 255, -511, 511);
  ctrl[1].lx = map(buf[4], 0, 255, -511, 511);
  ctrl[1].ly = map(buf[5], 0, 255, -511, 511);

  // Unpack button bitmasks
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
    // Note: updateEdges() is called separately in loop() after receive
  }
}

// ============================================================
// SEND TO ESP32
// ============================================================

void sendToESP32() {
  uint16_t grid1 = 0, grid2 = 0;
  uint8_t  beams = 0;

  // Re-encode grids from player state for transmission
  for (int r = 0; r < GRID_ROWS; r++)
    for (int c = 0; c < GRID_COLS; c++) {
      if (players[0].switchGrid[r][c]) grid1 |= (1 << (15 - (r * GRID_COLS + c)));
      if (players[1].switchGrid[r][c]) grid2 |= (1 << (15 - (r * GRID_COLS + c)));
    }

  uint8_t g1h = (grid1 >> 8) & 0xFF;
  uint8_t g1l =  grid1       & 0xFF;
  uint8_t g2h = (grid2 >> 8) & 0xFF;
  uint8_t g2l =  grid2       & 0xFF;
  uint8_t checksum = g1h ^ g1l ^ g2h ^ g2l ^ beams;

  SERIAL_ESP.write(START_BYTE);
  SERIAL_ESP.write(g1h);
  SERIAL_ESP.write(g1l);
  SERIAL_ESP.write(g2h);
  SERIAL_ESP.write(g2l);
  SERIAL_ESP.write(beams);
  SERIAL_ESP.write(checksum);
  SERIAL_ESP.write(END_BYTE);
}

// ============================================================
// SHIFT REGISTER PIN SETUP
// ============================================================

void setupShiftRegisterPins() {
  pinMode(P1_GRID_LOAD, OUTPUT); pinMode(P1_GRID_CLK, OUTPUT); pinMode(P1_GRID_DATA, INPUT);
  pinMode(P2_GRID_LOAD, OUTPUT); pinMode(P2_GRID_CLK, OUTPUT); pinMode(P2_GRID_DATA, INPUT);
  pinMode(BEAM_LOAD,    OUTPUT); pinMode(BEAM_CLK,    OUTPUT); pinMode(BEAM_DATA,    INPUT);

  digitalWrite(P1_GRID_LOAD, HIGH); digitalWrite(P1_GRID_CLK, LOW);
  digitalWrite(P2_GRID_LOAD, HIGH); digitalWrite(P2_GRID_CLK, LOW);
  digitalWrite(BEAM_LOAD,    HIGH); digitalWrite(BEAM_CLK,    LOW);
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  SERIAL_ESP.begin(BAUD_RATE);

  setupShiftRegisterPins();
  displayInit();
  ledsInit();

  // Initialise players
  for (int p = 0; p < NUM_PLAYERS; p++) {
    players[p].init();
  }

  // Initialise controller state — zero everything including prev states
  for (int c = 0; c < NUM_PLAYERS; c++) {
    ctrl[c].connected      = false;
    ctrl[c].lx = ctrl[c].ly = 0;
    ctrl[c].cross = ctrl[c].circle = ctrl[c].square = ctrl[c].triangle = false;
    ctrl[c].l1 = ctrl[c].r1 = ctrl[c].l2 = ctrl[c].r2 = false;
    ctrl[c].crossPressed = ctrl[c].circlePressed = false;
    ctrl[c].squarePressed = ctrl[c].trianglePressed = false;
    ctrl[c].l1Pressed = ctrl[c].r1Pressed = false;
    ctrl[c]._prevCross = ctrl[c]._prevCircle = false;
    ctrl[c]._prevSquare = ctrl[c]._prevTriangle = false;
    ctrl[c]._prevL1 = ctrl[c]._prevR1 = false;
  }

  setupPhaseInit();
  Serial.println("Battleship Mega ready.");
  Serial.printf("Ships per player: %d\n", NUM_SHIPS);
  for (int i = 0; i < NUM_SHIPS; i++) {
    Serial.printf("  Ship %d: %s (%d cells)\n", i, SHIP_NAMES[i], SHIP_SIZES[i]);
  }
}

// ============================================================
// MAIN LOOP
// ============================================================

unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 20;  // Send to ESP32 at ~50Hz

void loop() {
  // 1. Receive latest controller state from ESP32
  receiveFromESP32();

  // 2. Update edge detection IMMEDIATELY after receive
  //    This must happen before any phase update reads crossPressed etc.
  ctrl[0].updateEdges();
  ctrl[1].updateEdges();

  // 3. Send sensor state to ESP32 at ~50Hz
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = millis();
    updatePlayerGrid(0);
    updatePlayerGrid(1);
    sendToESP32();
  }

  // 4. Run current game phase
  switch (gamePhase) {
    case PHASE_SETUP:
      setupPhaseUpdate();
      break;

    case PHASE_PLAYING:
      turnPhaseUpdate();
      break;

    case PHASE_GAME_OVER:
      // Sit on end screen — power cycle to reset
      // To add a reset: check for a button press here and call setup()
      break;

    default:
      break;
  }
}
