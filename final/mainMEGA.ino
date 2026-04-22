// ─── INCLUDES ─────────────────────────────────────────────────────────────────
#include <Adafruit_NeoPixel.h>

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

// ─── BEAM GRID FUNCTIONS ──────────────────────────────────────────────────────
// Pins 2-9: beam breaks (4 row + 4 col)

const int BEAM_ROW_PINS[4] = {9, 8, 7, 6};
const int BEAM_COL_PINS[4] = {2, 3, 4, 5};
const int BEAM_WINDOW = 1000;

unsigned long lastBeamRowBreak[4] = {0, 0, 0, 0};
unsigned long lastBeamColBreak[4] = {0, 0, 0, 0};

struct GridHit {
  bool detected;
  int row;  // 1-4
  int col;  // 1-4
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

// ─── SHIP PLACEMENT FUNCTIONS ─────────────────────────────────────────────────
// ─── SHIP PLACEMENT ───────────────────────────────────────────────────────────

#define ROWS             4
#define COLS             4
#define SHIPS_PER_PLAYER 4
#define PRESS_WINDOW     1000

const char* SHIP_NAMES[SHIPS_PER_PLAYER] = { "Destroyer A", "Destroyer B", "Scout A", "Scout B" };
const int   SHIP_SIZES[SHIPS_PER_PLAYER] = { 2, 2, 1, 1 };

// Pin mapping
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

// Ship state arrays - used by watchBeamBreak() during gameplay
bool p1Ships[ROWS][COLS] = {};
bool p2Ships[ROWS][COLS] = {};
int  p1ShipIndex[ROWS][COLS];
int  p2ShipIndex[ROWS][COLS];

// Placement confirm/cancel flags set by handleESP32Serial()
bool confirmReceived = false;
bool cancelReceived  = false;

// ── LED helpers for placement ─────────────────────────────────────────────────

int placementCellToLED(int row, int col) {
  return (row * COLS + col) * LEDS_PER_CELL;
}

void setPlacementCell(Adafruit_NeoPixel &strip, int row, int col, uint32_t color) {
  int start = placementCellToLED(row, col);
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

// ── Placement serial helpers ──────────────────────────────────────────────────

void sendPlacementPrompt(const char* msg) {
  Serial1.print("PROMPT,");
  Serial1.println(msg);
}

void sendBoardToESP32(int player, bool ships[ROWS][COLS]) {
  Serial1.print("BOARD,");
  Serial1.print(player);
  Serial1.print(",");
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      Serial1.print(ships[r][c] ? "1" : "0");
  Serial1.println();
}

// ── Placement debug print ─────────────────────────────────────────────────────

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

// ── Read microswitches for one board ─────────────────────────────────────────

int readShipPlacement(const int pins[ROWS][COLS], bool occupied[ROWS][COLS],
                      bool pendingCells[ROWS][COLS]) {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      pendingCells[r][c] = false;

  Serial.println("Waiting for first cell press...");
  int firstRow = -1, firstCol = -1;

  while (firstRow == -1) {
    // Check for cancel from ESP32 while waiting
    if (Serial1.available()) {
      String msg = Serial1.readStringUntil('\n');
      msg.trim();
      msg.toUpperCase();
      if (msg == "CANCEL") return -1;
    }
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

    char promptBuf[40];
    snprintf(promptBuf, sizeof(promptBuf), "P%d PLACE %s", player, SHIP_NAMES[shipNum]);
    sendPlacementPrompt(promptBuf);

    bool pendingCells[ROWS][COLS] = {};
    int count = readShipPlacement(pins, ships, pendingCells);

    if (count == -1 || cancelReceived) {
      for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
          if (pendingCells[r][c]) setPlacementCell(strip, r, c, 0x000000);
      sendPlacementPrompt("CANCELLED TRY AGAIN");
      Serial.println("Cancelled, try again.");
      continue;
    }

    // Light pending cells blue
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (pendingCells[r][c]) setPlacementCell(strip, r, c, 0x0000FF);

    Serial.print("Cells lit: ");
    Serial.println(count);

    char confirmBuf[40];
    snprintf(confirmBuf, sizeof(confirmBuf), "P%d X=CONFIRM O=CANCEL", player);
    sendPlacementPrompt(confirmBuf);
    Serial.println("Waiting for confirm (X) or cancel (O)...");

    // Wait for response - read serial here since we're blocking
    while (!confirmReceived && !cancelReceived) {
      if (Serial1.available()) {
        String msg = Serial1.readStringUntil('\n');
        msg.trim();
        msg.toUpperCase();
        if (msg == "CONFIRM") confirmReceived = true;
        if (msg == "CANCEL")  cancelReceived  = true;
      }
      delay(20);
    }

    if (cancelReceived) {
      for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
          if (pendingCells[r][c]) setPlacementCell(strip, r, c, 0x000000);
      sendPlacementPrompt("CANCELLED TRY AGAIN");
      Serial.println("Cancelled, place again.");
      continue;
    }

    // Confirmed
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
    sendBoardToESP32(player, ships);
    debugPrintBoard(player == 1 ? "Player 1" : "Player 2", ships);

    Serial.print("Confirmed! ");
    Serial.print(SHIPS_PER_PLAYER - shipsPlaced);
    Serial.println(" ships remaining.");
  }

  char doneBuf[30];
  snprintf(doneBuf, sizeof(doneBuf), "P%d ALL SHIPS PLACED", player);
  sendPlacementPrompt(doneBuf);
  Serial.print("Player ");
  Serial.print(player);
  Serial.println(": All ships placed!");
  debugPrintBoard(player == 1 ? "Player 1" : "Player 2", ships);
}

// ── Main placement entry point ────────────────────────────────────────────────

void runPlacementPhase() {
  memset(p1ShipIndex, -1, sizeof(p1ShipIndex));
  memset(p2ShipIndex, -1, sizeof(p2ShipIndex));

  Serial.println("\n=== SHIP PLACEMENT PHASE ===");

  sendPlacementPrompt("P1 PLACE YOUR SHIPS");
  placeShipsForPlayer(1, P1_PINS, p1Ships, p1ShipIndex, p1AttackStrip);

  delay(2000);

  sendPlacementPrompt("P2 PLACE YOUR SHIPS");
  placeShipsForPlayer(2, P2_PINS, p2Ships, p2ShipIndex, p2AttackStrip);

  delay(1000);
  Serial.println("\n=== ALL SHIPS PLACED - GAME STARTING ===");
  sendPlacementPrompt("GAME STARTING");
}

// ─── LED GAMEPLAY FUNCTIONS ───────────────────────────────────────────────────

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

  Serial1.print(isHit ? "HIT," : "MISS,");
  Serial1.print(hit.row);
  Serial1.print(",");
  Serial1.println(hit.col);

  Serial.print(isHit ? "HIT" : "MISS");
  Serial.print(" at R");
  Serial.print(hit.row);
  Serial.print("C");
  Serial.println(hit.col);
}

// ─── ESP32 SERIAL HANDLER ─────────────────────────────────────────────────────

void handleESP32Serial() {
  if (!Serial1.available()) return;

  String msg = Serial1.readStringUntil('\n');
  msg.trim();
  msg.toUpperCase();

  Serial.print("ESP32: ");
  Serial.println(msg);

  if (msg == "GAME START") {
    gameActive = true;
    clearAllLEDs();
    Serial.println("Game started, watching beams.");
    return;
  }

  if (msg.startsWith("TURN,")) {
    currentShooter = msg.substring(5).toInt();
    Serial.print("Current shooter: P");
    Serial.println(currentShooter);
    return;
  }

  if (msg.startsWith("RESULT,")) {
    int player = msg.charAt(7) - '0';
    int isHit  = msg.charAt(9) - '0';
    int row    = msg.substring(11, 12).toInt();
    int col    = msg.substring(13).toInt();
    if (row >= 0 && col >= 0) {
      lightResultLED(player, isHit == 1, row, col);
    }
    return;
  }

  if (msg == "GAME OVER") {
    gameActive = false;
    clearAllLEDs();
    return;
  }

  if (msg == "RESET") {
    gameActive = false;
    clearAllLEDs();
    return;
  }
}

// ─── SETUP ────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);

  beamGridBegin();

  p1AttackStrip.begin(); p1AttackStrip.setBrightness(80); p1AttackStrip.clear(); p1AttackStrip.show();
  p2AttackStrip.begin(); p2AttackStrip.setBrightness(80); p2AttackStrip.clear(); p2AttackStrip.show();
  p1DefendStrip.begin(); p1DefendStrip.setBrightness(80); p1DefendStrip.clear(); p1DefendStrip.show();
  p2DefendStrip.begin(); p2DefendStrip.setBrightness(80); p2DefendStrip.clear(); p2DefendStrip.show();

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
