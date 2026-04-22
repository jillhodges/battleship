#include <Adafruit_NeoPixel.h>
#include "CommProtocol.h"

uint8_t serialBuf[22];
uint8_t serialBufLen = 0;

// ─── LED DEFINES ──────────────────────────────────────────────────────────────
#define P1_ATTACKER_PIN  10
#define P2_ATTACKER_PIN  11
#define P1_DEFENDER_PIN  12
#define P2_DEFENDER_PIN  13
#define LEDS_PER_CELL    7
#define TOTAL_LEDS       (16 * LEDS_PER_CELL)
#define strip_RED        0xFF0000
#define strip_BLUE       0x0000FF

Adafruit_NeoPixel p1AttackStrip(TOTAL_LEDS, P1_ATTACKER_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel p2AttackStrip(TOTAL_LEDS, P2_ATTACKER_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel p1DefendStrip(TOTAL_LEDS, P1_DEFENDER_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel p2DefendStrip(TOTAL_LEDS, P2_DEFENDER_PIN, NEO_GRB + NEO_KHZ800);

// ─── GAME STATE ───────────────────────────────────────────────────────────────
bool gameActive     = false;
int  currentShooter = 1;

// ─── BEAM GRID ────────────────────────────────────────────────────────────────
const int BEAM_ROW_PINS[4] = {9, 8, 7, 6};
const int BEAM_COL_PINS[4] = {2, 3, 4, 5};
const int BEAM_WINDOW = 1000;

unsigned long lastBeamRowBreak[4] = {0, 0, 0, 0};
unsigned long lastBeamColBreak[4] = {0, 0, 0, 0};

struct GridHit {
  bool detected;
  int row;
  int col;
};

void beamGridBegin() {
  for (int i = 0; i < 4; i++) {
    pinMode(BEAM_ROW_PINS[i], INPUT_PULLUP);
    pinMode(BEAM_COL_PINS[i], INPUT_PULLUP);
  }
  Serial.println("Beam grid ready.");
}

GridHit beamGridCheck() {
  GridHit result = {false, 0, 0};
  unsigned long now = millis();

  for (int i = 0; i < 4; i++) {
    if (digitalRead(BEAM_ROW_PINS[i]) == LOW) lastBeamRowBreak[i] = now;
    if (digitalRead(BEAM_COL_PINS[i]) == LOW) lastBeamColBreak[i] = now;
  }

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if ((now - lastBeamRowBreak[r] < BEAM_WINDOW) &&
          (now - lastBeamColBreak[c] < BEAM_WINDOW) &&
          lastBeamRowBreak[r] != 0 &&
          lastBeamColBreak[c] != 0) {
        result.detected = true;
        result.row = r + 1;
        result.col = c + 1;
        lastBeamRowBreak[r] = 0;
        lastBeamColBreak[c] = 0;
        return result;
      }
    }
  }
  return result;
}

// ─── SHIP PLACEMENT ───────────────────────────────────────────────────────────
#define ROWS             4
#define COLS             4
#define SHIPS_PER_PLAYER 4
#define PRESS_WINDOW     1000

const char* SHIP_NAMES[SHIPS_PER_PLAYER] = {
  "Destroyer A", "Destroyer B", "Scout A", "Scout B"
};
const int SHIP_SIZES[SHIPS_PER_PLAYER] = {2, 2, 1, 1};

const int P1_PINS[ROWS][COLS] = {
  {22, 23, 24, 25},
  {26, 27, 28, 29},
  {30, 31, 32, 33},
  {34, 35, 36, 37}
};
const int P2_PINS[ROWS][COLS] = {
  {38, 39, 40, 41},
  {42, 43, 44, 45},
  {46, 47, 48, 49},
  {50, 51, 52, 53}
};

bool p1Ships[ROWS][COLS]     = {};
bool p2Ships[ROWS][COLS]     = {};
int  p1ShipIndex[ROWS][COLS] = {};
int  p2ShipIndex[ROWS][COLS] = {};

bool confirmReceived = false;
bool cancelReceived  = false;

// ── Placement LED helpers ─────────────────────────────────────────────────────

void setPlacementCell(Adafruit_NeoPixel &strip, int row, int col, uint32_t color) {
  int start = (row * COLS + col) * LEDS_PER_CELL;
  for (int i = 0; i < LEDS_PER_CELL; i++)
    strip.setPixelColor(start + i, color);
  strip.show();
}

void flashPlacementCells(Adafruit_NeoPixel &strip, bool cells[ROWS][COLS],
                         uint32_t color, int times) {
  for (int t = 0; t < times; t++) {
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (cells[r][c]) setPlacementCell(strip, r, c, color);
    delay(120);
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (cells[r][c]) setPlacementCell(strip, r, c, 0x000000);
    delay(80);
  }
}

// ── Placement debug ───────────────────────────────────────────────────────────

void debugPrintBoard(const char* playerName, bool ships[ROWS][COLS]) {
  Serial.print("\n--- ");
  Serial.print(playerName);
  Serial.println(" Board ---");
  Serial.println("  C0  C1  C2  C3");
  for (int r = 0; r < ROWS; r++) {
    Serial.print("R");
    Serial.print(r);
    Serial.print(" ");
    for (int c = 0; c < COLS; c++)
      Serial.print(ships[r][c] ? "[X] " : "[ ] ");
    Serial.println();
  }
  Serial.println();
}

// ── Read microswitches ────────────────────────────────────────────────────────

int readShipPlacement(const int pins[ROWS][COLS], bool occupied[ROWS][COLS],
                      bool pendingCells[ROWS][COLS]) {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      pendingCells[r][c] = false;

  Serial.println("Waiting for first cell press...");
  int firstRow = -1, firstCol = -1;

  while (firstRow == -1) {
    // Check for cancel while waiting
    ParsedMsg msg = receiveMsg(Serial1);
    if (msg.valid && msg.type == MSG_CANCEL) return -1;

    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        if (digitalRead(pins[r][c]) == HIGH && !occupied[r][c]) {
          firstRow = r;
          firstCol = c;
        }
      }
    }
    delay(20);
  }

  unsigned long windowStart = millis();
  pendingCells[firstRow][firstCol] = true;
  int count = 1;

  Serial.print("First press R");
  Serial.print(firstRow);
  Serial.print("C");
  Serial.println(firstCol);

  while (millis() - windowStart < PRESS_WINDOW) {
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        if (digitalRead(pins[r][c]) == HIGH
            && !occupied[r][c]
            && !pendingCells[r][c]) {
          pendingCells[r][c] = true;
          count++;
          Serial.print("Additional press R");
          Serial.print(r);
          Serial.print("C");
          Serial.println(c);
        }
      }
    }
    delay(20);
  }
  return count;
}

// ── Place ships for one player ────────────────────────────────────────────────

void placeShipsForPlayer(int player,
                         const int pins[ROWS][COLS],
                         bool ships[ROWS][COLS],
                         int shipIndex[ROWS][COLS],
                         Adafruit_NeoPixel &strip) {
  int shipsPlaced = 0;

  while (shipsPlaced < SHIPS_PER_PLAYER) {
    confirmReceived = false;
    cancelReceived  = false;

    int shipNum = shipsPlaced;

    Serial.print("\nP");
    Serial.print(player);
    Serial.print(": Place ship ");
    Serial.print(shipsPlaced + 1);
    Serial.print("/");
    Serial.print(SHIPS_PER_PLAYER);
    Serial.print(" - ");
    Serial.print(SHIP_NAMES[shipNum]);
    Serial.print(" (");
    Serial.print(SHIP_SIZES[shipNum]);
    Serial.println(" cells)");

    // Send prompt code to ESP32
    uint8_t promptCode = (player == 1) ? PROMPT_P1_PLACE : PROMPT_P2_PLACE;
    sendPrompt(Serial1, promptCode);

    bool pendingCells[ROWS][COLS] = {};
    int count = readShipPlacement(pins, ships, pendingCells);

    if (count == -1 || cancelReceived) {
      for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
          if (pendingCells[r][c]) setPlacementCell(strip, r, c, 0x000000);
      uint8_t cc = (player == 1) ? PROMPT_P1_CANCELLED : PROMPT_P2_CANCELLED;
      sendPrompt(Serial1, cc);
      Serial.println("Cancelled, try again.");
      continue;
    }

    // Light pending cells blue
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (pendingCells[r][c]) setPlacementCell(strip, r, c, 0x0000FF);

    Serial.print("Cells lit: ");
    Serial.println(count);

    uint8_t cp = (player == 1) ? PROMPT_P1_CONFIRM : PROMPT_P2_CONFIRM;
    sendPrompt(Serial1, cp);
    Serial.println("Waiting for X or O...");

    // Wait for confirm or cancel
    while (!confirmReceived && !cancelReceived) {
      ParsedMsg msg = receiveMsg(Serial1);
      if (msg.valid) {
        if (msg.type == MSG_CONFIRM) confirmReceived = true;
        if (msg.type == MSG_CANCEL)  cancelReceived  = true;
      }
      delay(20);
    }

    if (cancelReceived) {
      for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
          if (pendingCells[r][c]) setPlacementCell(strip, r, c, 0x000000);
      uint8_t cc = (player == 1) ? PROMPT_P1_CANCELLED : PROMPT_P2_CANCELLED;
      sendPrompt(Serial1, cc);
      Serial.println("Cancelled, place again.");
      continue;
    }

    // Confirmed - flash orange then set final color
    flashPlacementCells(strip, pendingCells, 0xFF8800, 2);

    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        if (pendingCells[r][c]) {
          ships[r][c]     = true;
          shipIndex[r][c] = shipNum;
          uint32_t color  = (SHIP_SIZES[shipNum] == 1) ? 0x00FFFF : 0xFF00FF;
          setPlacementCell(strip, r, c, color);
        }
      }
    }

    shipsPlaced++;
    sendBoard(Serial1, player, ships);
    debugPrintBoard(player == 1 ? "Player 1" : "Player 2", ships);

    Serial.print("Confirmed! ");
    Serial.print(SHIPS_PER_PLAYER - shipsPlaced);
    Serial.println(" ships remaining.");
  }

  uint8_t doneCode = (player == 1) ? PROMPT_P1_DONE : PROMPT_P2_DONE;
  sendPrompt(Serial1, doneCode);
  Serial.print("Player ");
  Serial.print(player);
  Serial.println(": All ships placed!");
  debugPrintBoard(player == 1 ? "Player 1" : "Player 2", ships);
}

// ── Main placement phase ──────────────────────────────────────────────────────

void runPlacementPhase() {
  memset(p1ShipIndex, -1, sizeof(p1ShipIndex));
  memset(p2ShipIndex, -1, sizeof(p2ShipIndex));
  memset(p1Ships, 0, sizeof(p1Ships));
  memset(p2Ships, 0, sizeof(p2Ships));

  Serial.println("\n=== SHIP PLACEMENT PHASE ===");

  sendPrompt(Serial1, PROMPT_P1_PLACE);
  placeShipsForPlayer(1, P1_PINS, p1Ships, p1ShipIndex, p1AttackStrip);

  delay(2000);

  sendPrompt(Serial1, PROMPT_P2_PLACE);
  placeShipsForPlayer(2, P2_PINS, p2Ships, p2ShipIndex, p2AttackStrip);

  delay(1000);
  Serial.println("\n=== ALL SHIPS PLACED ===");
  sendPrompt(Serial1, PROMPT_GAME_STARTING);
}

// ─── LED GAMEPLAY ─────────────────────────────────────────────────────────────

int cellToLED(int row, int col) {
  return (row * 4 + col) * LEDS_PER_CELL;
}

void setStripCell(Adafruit_NeoPixel &strip, int row, int col, uint32_t color) {
  int start = cellToLED(row, col);
  for (int i = 0; i < LEDS_PER_CELL; i++)
    strip.setPixelColor(start + i, color);
  strip.show();
}

void lightResultLED(int shootingPlayer, bool isHit, int row, int col) {
  uint32_t color = isHit ? strip_RED : strip_BLUE;
  if (shootingPlayer == 1) {
    setStripCell(p1AttackStrip, row, col, color);
    setStripCell(p2DefendStrip, row, col, color);
  } else {
    setStripCell(p2AttackStrip, row, col, color);
    setStripCell(p1DefendStrip, row, col, color);
  }
}

void clearAllLEDs() {
  p1AttackStrip.clear(); p1AttackStrip.show();
  p2AttackStrip.clear(); p2AttackStrip.show();
  p1DefendStrip.clear(); p1DefendStrip.show();
  p2DefendStrip.clear(); p2DefendStrip.show();
}

// ─── BEAM BREAK WATCHER ───────────────────────────────────────────────────────

void watchBeamBreak() {
  if (!gameActive) return;

  GridHit hit = beamGridCheck();
  if (!hit.detected) return;

  int defenderPlayer = (currentShooter == 1) ? 2 : 1;
  bool (*defenderBoard)[4] = (defenderPlayer == 1) ? p1Ships : p2Ships;
  bool isHit = defenderBoard[hit.row - 1][hit.col - 1];

  if (isHit) {
    sendHit(Serial1, hit.row, hit.col);
  } else {
    sendMiss(Serial1, hit.row, hit.col);
  }

  Serial.print(isHit ? "HIT" : "MISS");
  Serial.print(" at R");
  Serial.print(hit.row);
  Serial.print("C");
  Serial.println(hit.col);
}

// ─── ESP32 SERIAL HANDLER ─────────────────────────────────────────────────────

void handleESP32Serial() {
  ParsedMsg msg = receiveMsg(Serial1);
  if (!msg.valid) return;

  switch (msg.type) {

    case MSG_CONFIRM:
      confirmReceived = true;
      Serial.println("CONFIRM");
      break;

    case MSG_CANCEL:
      cancelReceived = true;
      Serial.println("CANCEL");
      break;

    case MSG_GAME_START:
      gameActive = true;
      clearAllLEDs();
      Serial.println("Game started.");
      break;

    case MSG_TURN:
      currentShooter = msg.payload[0];
      Serial.print("Turn: P");
      Serial.println(currentShooter);
      break;

    case MSG_RESULT: {
      int player = msg.payload[0];
      bool isHit = msg.payload[1] == 1;
      int row    = msg.payload[2];
      int col    = msg.payload[3];
      if (row > 0 && col > 0) {
        lightResultLED(player, isHit, row, col);
      }
      break;
    }

    case MSG_GAME_OVER:
      gameActive = false;
      clearAllLEDs();
      Serial.println("Game over.");
      break;

    case MSG_RESET:
      gameActive = false;
      clearAllLEDs();
      Serial.println("Reset.");
      break;

    default:
      Serial.print("Unknown msg: 0x");
      Serial.println(msg.type, HEX);
      break;
  }
}

// ─── SETUP ────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);

  beamGridBegin();

  p1AttackStrip.begin(); p1AttackStrip.setBrightness(80);
  p1AttackStrip.clear(); p1AttackStrip.show();
  p2AttackStrip.begin(); p2AttackStrip.setBrightness(80);
  p2AttackStrip.clear(); p2AttackStrip.show();
  p1DefendStrip.begin(); p1DefendStrip.setBrightness(80);
  p1DefendStrip.clear(); p1DefendStrip.show();
  p2DefendStrip.begin(); p2DefendStrip.setBrightness(80);
  p2DefendStrip.clear(); p2DefendStrip.show();

  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      pinMode(P1_PINS[r][c], INPUT_PULLUP);

  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      pinMode(P2_PINS[r][c], INPUT_PULLUP);

  runPlacementPhase();
}

// ─── LOOP ─────────────────────────────────────────────────────────────────────

void loop() {
  handleESP32Serial();
  watchBeamBreak();
}
