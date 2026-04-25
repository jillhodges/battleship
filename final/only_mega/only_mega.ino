// only_mega.ino
//
// Standalone Battleship controller for the Arduino Mega.
// No ESP32, no Serial1, no CommProtocol — the Mega runs the entire game on its own.
//
// Flow:
//   1. Placement phase: each player presses microswitches to place 3 ships
//      (sizes 2, 2, 1). The Mega auto-confirms a ship when the correct
//      number of cells is pressed inside the press window AND they form a
//      contiguous straight line. Wrong count / non-contiguous = retry.
//      Each placed ship gets a unique ship index so individual ships can
//      be tracked and "sunk" detected.
//   2. When both players are done, ALL leds across all 4 strips flash a
//      few times to announce the placement phase has ended and the game
//      has begun.
//   3. Gameplay: beam-break grid detects shots. On a hit, the shooter's
//      attacker board AND the defender's board light RED at that cell.
//      On a miss they light WHITE. When every cell of a ship is hit, the
//      ship is SUNK — its cells flash red on the defender's board and the
//      shooter's attack board. Turns alternate after every shot. Game
//      ends when one player's ships are all sunk; winner's defender board
//      flashes red, then a new placement phase begins.

#include <Adafruit_NeoPixel.h>

// ─── LED CONFIG ───────────────────────────────────────────────────────────────
#define P1_ATTACKER_PIN  10
#define P2_ATTACKER_PIN  11
#define P1_DEFENDER_PIN  12
#define P2_DEFENDER_PIN  13
#define LEDS_PER_CELL    7
#define ROWS             4
#define COLS             4
#define CELLS            (ROWS * COLS)
#define TOTAL_LEDS       (CELLS * LEDS_PER_CELL)

#define COLOR_RED        0xFF0000
#define COLOR_WHITE      0xFFFFFF
#define COLOR_BLUE       0x0000FF
#define COLOR_ORANGE     0xFF8800
#define COLOR_GREEN      0x00FF00
#define COLOR_DESTROYER  0xFF00FF  // 2-cell ship color
#define COLOR_SCOUT      0x00FFFF  // 1-cell ship color
#define COLOR_OFF        0x000000

Adafruit_NeoPixel p1AttackStrip(TOTAL_LEDS, P1_ATTACKER_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel p2AttackStrip(TOTAL_LEDS, P2_ATTACKER_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel p1DefendStrip(TOTAL_LEDS, P1_DEFENDER_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel p2DefendStrip(TOTAL_LEDS, P2_DEFENDER_PIN, NEO_GRB + NEO_KHZ800);

Adafruit_NeoPixel* ALL_STRIPS[4] = {
  &p1AttackStrip, &p2AttackStrip, &p1DefendStrip, &p2DefendStrip
};

// ─── SHIP CONFIG ──────────────────────────────────────────────────────────────
#define SHIPS_PER_PLAYER 3
const char* SHIP_NAMES[SHIPS_PER_PLAYER] = {
  "Destroyer A", "Destroyer B", "Scout"
};
const int SHIP_SIZES[SHIPS_PER_PLAYER] = {2, 2, 1};
#define TOTAL_SHIP_CELLS 5  // 2+2+1

// ─── PLACEMENT PINS ───────────────────────────────────────────────────────────
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

#define PRESS_WINDOW 1500  // ms — how long to keep collecting cell presses

// ─── BEAM GRID ────────────────────────────────────────────────────────────────
const int BEAM_ROW_PINS[4] = {9, 8, 7, 6};
const int BEAM_COL_PINS[4] = {2, 3, 4, 5};
const unsigned long BEAM_WINDOW = 1000;
unsigned long lastBeamRowBreak[4] = {0, 0, 0, 0};
unsigned long lastBeamColBreak[4] = {0, 0, 0, 0};

struct GridHit { bool detected; int row; int col; };

// ─── GAME STATE ───────────────────────────────────────────────────────────────
bool p1Ships[ROWS][COLS]    = {};
bool p2Ships[ROWS][COLS]    = {};
bool p1Hits[ROWS][COLS]     = {};
bool p2Hits[ROWS][COLS]     = {};
// Per-cell ship index: which ship sits in each cell (-1 = empty). Lets us
// detect when every cell of a specific ship has been hit (sunk).
int  p1ShipIdx[ROWS][COLS]  = {};
int  p2ShipIdx[ROWS][COLS]  = {};
int  p1ShipHits[SHIPS_PER_PLAYER] = {};
int  p2ShipHits[SHIPS_PER_PLAYER] = {};
bool p1ShipSunk[SHIPS_PER_PLAYER] = {};
bool p2ShipSunk[SHIPS_PER_PLAYER] = {};
int  p1SunkCount = 0;
int  p2SunkCount = 0;
int  p1HitCount = 0;
int  p2HitCount = 0;
int  currentShooter = 1;
bool gameActive = false;
unsigned long lastShotTime = 0;
const unsigned long SHOT_COOLDOWN = 800;  // debounce between accepted shots

// ─── LED HELPERS ──────────────────────────────────────────────────────────────
void setStripCell(Adafruit_NeoPixel &strip, int row, int col, uint32_t color) {
  int start = (row * COLS + col) * LEDS_PER_CELL;
  for (int i = 0; i < LEDS_PER_CELL; i++) strip.setPixelColor(start + i, color);
  strip.show();
}

void clearAllLEDs() {
  for (int i = 0; i < 4; i++) { ALL_STRIPS[i]->clear(); ALL_STRIPS[i]->show(); }
}

void fillAllLEDs(uint32_t color) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < TOTAL_LEDS; j++) ALL_STRIPS[i]->setPixelColor(j, color);
    ALL_STRIPS[i]->show();
  }
}

void flashAllLEDs(uint32_t color, int times, int onMs, int offMs) {
  for (int t = 0; t < times; t++) {
    fillAllLEDs(color);
    delay(onMs);
    fillAllLEDs(COLOR_OFF);
    delay(offMs);
  }
}

void flashStripCells(Adafruit_NeoPixel &strip, bool cells[ROWS][COLS],
                     uint32_t color, int times) {
  for (int t = 0; t < times; t++) {
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (cells[r][c]) setStripCell(strip, r, c, color);
    delay(120);
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (cells[r][c]) setStripCell(strip, r, c, COLOR_OFF);
    delay(80);
  }
}

void flashWholeStrip(Adafruit_NeoPixel &strip, uint32_t color, int times) {
  for (int t = 0; t < times; t++) {
    for (int j = 0; j < TOTAL_LEDS; j++) strip.setPixelColor(j, color);
    strip.show();
    delay(180);
    strip.clear(); strip.show();
    delay(120);
  }
}

// Flash every cell that belongs to ship `shipNum` on `strip`, then leave
// those cells solid red.
void flashSunkShip(Adafruit_NeoPixel &strip, int shipIdx[ROWS][COLS],
                   int shipNum, int times) {
  bool cells[ROWS][COLS] = {};
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (shipIdx[r][c] == shipNum) cells[r][c] = true;
  flashStripCells(strip, cells, COLOR_RED, times);
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (cells[r][c]) setStripCell(strip, r, c, COLOR_RED);
}

// ─── BEAM GRID ────────────────────────────────────────────────────────────────
void beamGridBegin() {
  for (int i = 0; i < 4; i++) {
    pinMode(BEAM_ROW_PINS[i], INPUT_PULLUP);
    pinMode(BEAM_COL_PINS[i], INPUT_PULLUP);
  }
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
          lastBeamRowBreak[r] != 0 && lastBeamColBreak[c] != 0) {
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

// ─── DEBUG ────────────────────────────────────────────────────────────────────
void debugPrintBoard(const char* name, bool ships[ROWS][COLS]) {
  Serial.print("\n--- "); Serial.print(name); Serial.println(" Board ---");
  Serial.println("  C0  C1  C2  C3");
  for (int r = 0; r < ROWS; r++) {
    Serial.print("R"); Serial.print(r); Serial.print(" ");
    for (int c = 0; c < COLS; c++) Serial.print(ships[r][c] ? "[X] " : "[ ] ");
    Serial.println();
  }
  Serial.println();
}

// ─── PLACEMENT ────────────────────────────────────────────────────────────────
// Read presses for one ship. Returns the number of unique unoccupied cells
// pressed within PRESS_WINDOW ms after the first press. Cells are written
// into pendingCells.
int readShipPlacement(const int pins[ROWS][COLS], bool occupied[ROWS][COLS],
                      bool pendingCells[ROWS][COLS]) {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) pendingCells[r][c] = false;

  // Wait for first press.
  int firstRow = -1, firstCol = -1;
  while (firstRow == -1) {
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        if (digitalRead(pins[r][c]) == HIGH && !occupied[r][c]) {
          firstRow = r; firstCol = c;
        }
      }
    }
    delay(15);
  }

  unsigned long windowStart = millis();
  pendingCells[firstRow][firstCol] = true;
  int count = 1;

  while (millis() - windowStart < PRESS_WINDOW) {
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        if (digitalRead(pins[r][c]) == HIGH
            && !occupied[r][c]
            && !pendingCells[r][c]) {
          pendingCells[r][c] = true;
          count++;
        }
      }
    }
    delay(15);
  }
  return count;
}

// True iff the cells in `pending` form a contiguous straight line (horizontal
// or vertical). Single-cell ships always pass.
bool cellsAreContiguousLine(bool pending[ROWS][COLS], int expectedSize) {
  if (expectedSize == 1) return true;

  int rs[8], cs[8], n = 0;
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (pending[r][c]) { rs[n] = r; cs[n] = c; n++; }
  if (n != expectedSize) return false;

  bool sameRow = true, sameCol = true;
  for (int i = 1; i < n; i++) {
    if (rs[i] != rs[0]) sameRow = false;
    if (cs[i] != cs[0]) sameCol = false;
  }
  if (!sameRow && !sameCol) return false;

  // Sort the varying axis and check adjacency.
  int vals[8];
  for (int i = 0; i < n; i++) vals[i] = sameRow ? cs[i] : rs[i];
  for (int i = 0; i < n - 1; i++)
    for (int j = i + 1; j < n; j++)
      if (vals[j] < vals[i]) { int t = vals[i]; vals[i] = vals[j]; vals[j] = t; }
  for (int i = 1; i < n; i++) if (vals[i] != vals[i-1] + 1) return false;
  return true;
}

void placeShipsForPlayer(int player,
                         const int pins[ROWS][COLS],
                         bool ships[ROWS][COLS],
                         int shipIdx[ROWS][COLS],
                         Adafruit_NeoPixel &strip) {
  Serial.print("\n=== Player "); Serial.print(player); Serial.println(" placement ===");

  int placed = 0;
  while (placed < SHIPS_PER_PLAYER) {
    int expected = SHIP_SIZES[placed];
    Serial.print("Place "); Serial.print(SHIP_NAMES[placed]);
    Serial.print(" ("); Serial.print(expected); Serial.println(" cell(s))");

    bool pending[ROWS][COLS] = {};
    int count = readShipPlacement(pins, ships, pending);

    bool ok = (count == expected) && cellsAreContiguousLine(pending, expected);

    if (!ok) {
      // Reject: flash red on any pending cells, do not commit.
      Serial.print("Rejected (count="); Serial.print(count);
      Serial.println("). Re-place this ship.");
      flashStripCells(strip, pending, COLOR_RED, 3);
      for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
          if (pending[r][c]) setStripCell(strip, r, c, COLOR_OFF);
      continue;
    }

    // Accept: flash orange, then color by ship type, commit to board AND
    // record which ship index occupies each cell.
    flashStripCells(strip, pending, COLOR_ORANGE, 2);
    uint32_t finalColor = (expected == 1) ? COLOR_SCOUT : COLOR_DESTROYER;
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (pending[r][c]) {
          ships[r][c]   = true;
          shipIdx[r][c] = placed;
          setStripCell(strip, r, c, finalColor);
        }
    placed++;
    Serial.print("Confirmed. "); Serial.print(SHIPS_PER_PLAYER - placed);
    Serial.println(" ships left.");
    debugPrintBoard(player == 1 ? "P1" : "P2", ships);
    delay(400);
  }
}

void runPlacementPhase() {
  memset(p1Ships, 0, sizeof(p1Ships));
  memset(p2Ships, 0, sizeof(p2Ships));
  memset(p1Hits,  0, sizeof(p1Hits));
  memset(p2Hits,  0, sizeof(p2Hits));
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) { p1ShipIdx[r][c] = -1; p2ShipIdx[r][c] = -1; }
  for (int i = 0; i < SHIPS_PER_PLAYER; i++) {
    p1ShipHits[i] = 0; p2ShipHits[i] = 0;
    p1ShipSunk[i] = false; p2ShipSunk[i] = false;
  }
  p1SunkCount = 0; p2SunkCount = 0;
  p1HitCount = 0;  p2HitCount = 0;
  clearAllLEDs();

  Serial.println("\n=== SHIP PLACEMENT PHASE ===");
  placeShipsForPlayer(1, P1_PINS, p1Ships, p1ShipIdx, p1DefendStrip);
  delay(800);
  placeShipsForPlayer(2, P2_PINS, p2Ships, p2ShipIdx, p2DefendStrip);

  Serial.println("\n=== ALL SHIPS PLACED — GAME START ===");
  delay(400);
  flashAllLEDs(COLOR_GREEN, 4, 180, 140);
  flashAllLEDs(COLOR_WHITE, 2, 120, 100);
  clearAllLEDs();

  currentShooter = 1;
  gameActive = true;
  lastShotTime = 0;
  Serial.println("Player 1 shoots first.");
}

// ─── GAMEPLAY ─────────────────────────────────────────────────────────────────
void lightShotResult(int shooter, bool isHit, int row, int col) {
  uint32_t color = isHit ? COLOR_RED : COLOR_WHITE;
  if (shooter == 1) {
    setStripCell(p1AttackStrip, row, col, color);
    setStripCell(p2DefendStrip, row, col, color);
  } else {
    setStripCell(p2AttackStrip, row, col, color);
    setStripCell(p1DefendStrip, row, col, color);
  }
}

void announceSunk(int shooter, int defender, int shipNum) {
  Serial.print(">>> SUNK: P"); Serial.print(defender);
  Serial.print(" "); Serial.print(SHIP_NAMES[shipNum]); Serial.println(" <<<");

  Adafruit_NeoPixel &defStrip    = (defender == 1) ? p1DefendStrip : p2DefendStrip;
  Adafruit_NeoPixel &attackStrip = (shooter  == 1) ? p1AttackStrip : p2AttackStrip;
  int (*defShipIdx)[COLS]        = (defender == 1) ? p1ShipIdx     : p2ShipIdx;

  // Flash the sunk ship's cells on both the defender's board and the
  // shooter's attack board, then leave them solid red.
  flashSunkShip(defStrip,    defShipIdx, shipNum, 4);
  flashSunkShip(attackStrip, defShipIdx, shipNum, 1);
}

void endGame(int winner) {
  gameActive = false;
  Serial.print("\n=== GAME OVER — Player "); Serial.print(winner); Serial.println(" wins ===");
  Adafruit_NeoPixel &winStrip = (winner == 1) ? p1DefendStrip : p2DefendStrip;
  for (int t = 0; t < 5; t++) {
    flashWholeStrip(winStrip, COLOR_RED, 1);
    flashAllLEDs(COLOR_WHITE, 1, 100, 80);
  }
  clearAllLEDs();
  delay(1500);
  runPlacementPhase();
}

void watchBeamBreak() {
  if (!gameActive) return;
  if (millis() - lastShotTime < SHOT_COOLDOWN) {
    beamGridCheck();
    return;
  }

  GridHit hit = beamGridCheck();
  if (!hit.detected) return;

  int defender = (currentShooter == 1) ? 2 : 1;
  int r = hit.row - 1, c = hit.col - 1;

  bool (*defShips)[COLS] = (defender == 1) ? p1Ships : p2Ships;
  bool (*defHits)[COLS]  = (defender == 1) ? p1Hits  : p2Hits;
  int  (*defShipIdx)[COLS] = (defender == 1) ? p1ShipIdx : p2ShipIdx;
  int  *defShipHits      = (defender == 1) ? p1ShipHits : p2ShipHits;
  bool *defShipSunk      = (defender == 1) ? p1ShipSunk : p2ShipSunk;
  int  &defSunkCount     = (defender == 1) ? p1SunkCount : p2SunkCount;
  int  &defHitCount      = (defender == 1) ? p1HitCount  : p2HitCount;

  if (defHits[r][c]) {
    lastShotTime = millis();
    return;
  }
  defHits[r][c] = true;

  bool isHit = defShips[r][c];
  lightShotResult(currentShooter, isHit, r, c);

  Serial.print(isHit ? "HIT " : "MISS");
  Serial.print(" P"); Serial.print(currentShooter);
  Serial.print(" -> R"); Serial.print(hit.row);
  Serial.print("C"); Serial.println(hit.col);

  if (isHit) {
    defHitCount++;
    int shipNum = defShipIdx[r][c];
    if (shipNum >= 0 && shipNum < SHIPS_PER_PLAYER) {
      defShipHits[shipNum]++;
      if (!defShipSunk[shipNum] && defShipHits[shipNum] >= SHIP_SIZES[shipNum]) {
        defShipSunk[shipNum] = true;
        defSunkCount++;
        announceSunk(currentShooter, defender, shipNum);
      }
    }
    if (defSunkCount >= SHIPS_PER_PLAYER) {
      endGame(currentShooter);
      return;
    }
  }

  currentShooter = (currentShooter == 1) ? 2 : 1;
  lastShotTime = millis();
  Serial.print("Next: P"); Serial.println(currentShooter);
}

// ─── SETUP / LOOP ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  beamGridBegin();

  for (int i = 0; i < 4; i++) {
    ALL_STRIPS[i]->begin();
    ALL_STRIPS[i]->setBrightness(80);
    ALL_STRIPS[i]->clear();
    ALL_STRIPS[i]->show();
  }

  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) {
      pinMode(P1_PINS[r][c], INPUT_PULLUP);
      pinMode(P2_PINS[r][c], INPUT_PULLUP);
    }

  runPlacementPhase();
}

void loop() {
  watchBeamBreak();
}
