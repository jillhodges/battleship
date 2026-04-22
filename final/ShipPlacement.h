/*
 * ShipPlacement.h
 * 
 * Ships per player:
 *   Ship 1: 2 cells (2x1)
 *   Ship 2: 2 cells (2x1)
 *   Ship 3: 1 cell  (1x1)
 *   Ship 4: 1 cell  (1x1)
 * 
 * Player presses all cells for a ship (pins hit buttons near-simultaneously).
 * 1000ms window groups all presses into one ship.
 * Player confirms with X on PS4 controller (ESP32 sends "CONFIRM" via Serial1).
 * Player cancels with O (ESP32 sends "CANCEL").
 */

#ifndef SHIP_PLACEMENT_H
#define SHIP_PLACEMENT_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// ─── NeoPixel Config ──────────────────────────────────────────────────────────
#define P1_LED_PIN     10
#define P2_LED_PIN     11
#define LEDS_PER_CELL  7
#define GRID_CELLS     16
#define TOTAL_LEDS     (LEDS_PER_CELL * GRID_CELLS)  // 112 per strip

// ─── Grid Config ──────────────────────────────────────────────────────────────
#define ROWS           4
#define COLS           4
#define SHIPS_PER_PLAYER 3
#define PRESS_WINDOW   1000  // ms to group simultaneous presses

// ─── Ship Definitions ─────────────────────────────────────────────────────────
// Name and expected cell count (for display/debug only, no hard validation)
const char* SHIP_NAMES[SHIPS_PER_PLAYER]  = { "Destroyer A", "Scout A", "Scout B" };
const int   SHIP_SIZES[SHIPS_PER_PLAYER]  = { 2, 1, 1 };

// ─── Pin Mapping ──────────────────────────────────────────────────────────────
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

// ─── Colors ───────────────────────────────────────────────────────────────────
#define COLOR_OFF      0x000000
#define COLOR_PENDING  0x0000FF   // blue   - pressed, awaiting confirm
#define COLOR_PLACED   0x00FF00   // green  - confirmed
#define COLOR_CONFIRM  0xFF8800   // orange - flash on confirm
#define COLOR_ERROR    0xFF0000   // red    - overlap warning
#define COLOR_SCOUT    0x00FFFF   // cyan   - 1-cell ships
#define COLOR_DESTROY  0xFF00FF   // magenta - 2-cell ships

Adafruit_NeoPixel p1Strip(TOTAL_LEDS, P1_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel p2Strip(TOTAL_LEDS, P2_LED_PIN, NEO_GRB + NEO_KHZ800);

// ─── Placement State ──────────────────────────────────────────────────────────
bool p1Ships[ROWS][COLS] = {};
bool p2Ships[ROWS][COLS] = {};

// Which ship index each cell belongs to (-1 = empty)
int p1ShipIndex[ROWS][COLS];
int p2ShipIndex[ROWS][COLS];

bool confirmReceived = false;
bool cancelReceived  = false;

// ─── LED Helpers ──────────────────────────────────────────────────────────────

int cellToLEDIndex(int row, int col) {
  return (row * COLS + col) * LEDS_PER_CELL;
}

void setCellColor(Adafruit_NeoPixel &strip, int row, int col, uint32_t color) {
  int start = cellToLEDIndex(row, col);
  for (int i = 0; i < LEDS_PER_CELL; i++)
    strip.setPixelColor(start + i, color);
  strip.show();
}

void flashCell(Adafruit_NeoPixel &strip, int row, int col, uint32_t color, int times) {
  for (int i = 0; i < times; i++) {
    setCellColor(strip, row, col, color);
    delay(120);
    setCellColor(strip, row, col, COLOR_OFF);
    delay(80);
  }
}

// Flash all cells in a pending set
void flashPendingCells(Adafruit_NeoPixel &strip, bool pending[ROWS][COLS], uint32_t color, int times) {
  for (int t = 0; t < times; t++) {
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (pending[r][c]) setCellColor(strip, r, c, color);
    delay(120);
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (pending[r][c]) setCellColor(strip, r, c, COLOR_OFF);
    delay(80);
  }
}

// Redraw full confirmed board
void redrawBoard(Adafruit_NeoPixel &strip, bool ships[ROWS][COLS], int shipIndex[ROWS][COLS]) {
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      if (ships[r][c]) {
        // Color by ship type
        int idx = shipIndex[r][c];
        uint32_t color = (SHIP_SIZES[idx] == 1) ? COLOR_SCOUT : COLOR_DESTROY;
        setCellColor(strip, r, c, color);
      } else {
        setCellColor(strip, r, c, COLOR_OFF);
      }
    }
  }
}

// ─── Serial Helpers ───────────────────────────────────────────────────────────

void checkESP32Serial() {
  if (Serial1.available()) {
    String msg = Serial1.readStringUntil('\n');
    msg.trim();
    msg.toUpperCase();
    if (msg == "CONFIRM") confirmReceived = true;
    if (msg == "CANCEL")  cancelReceived  = true;
  }
}

void sendPromptToESP32(const char* msg) {
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

// ─── Debug ────────────────────────────────────────────────────────────────────

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

// ─── Board Reading ────────────────────────────────────────────────────────────

// Scan board, return all newly pressed cells within PRESS_WINDOW
// Fills pendingCells[ROWS][COLS] with true where pressed
// Returns count of pressed cells
int readShipPlacement(const int pins[ROWS][COLS], bool occupied[ROWS][COLS],
                      bool pendingCells[ROWS][COLS]) {

  // Clear pending
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      pendingCells[r][c] = false;

  // Wait for first press
  Serial.println("Waiting for first cell press...");
  int firstRow = -1, firstCol = -1;
  while (firstRow == -1) {
    checkESP32Serial();
    if (cancelReceived) return -1; // cancelled before placing
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

  // First cell detected - open the window
  unsigned long windowStart = millis();
  pendingCells[firstRow][firstCol] = true;
  int count = 1;

  Serial.print("First press at R");
  Serial.print(firstRow);
  Serial.print("C");
  Serial.println(firstCol);

  // Collect all additional presses within PRESS_WINDOW
  while (millis() - windowStart < PRESS_WINDOW) {
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        if (digitalRead(pins[r][c]) == HIGH
            && !occupied[r][c]
            && !pendingCells[r][c]) {
          pendingCells[r][c] = true;
          count++;
          Serial.print("Additional press at R");
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

// ─── Single Player Placement Loop ────────────────────────────────────────────

void placeShipsForPlayer(int player,
                         const int pins[ROWS][COLS],
                         bool ships[ROWS][COLS],
                         int shipIndex[ROWS][COLS],
                         Adafruit_NeoPixel &strip) {

  int shipsPlaced = 0;

  while (shipsPlaced < SHIPS_PER_PLAYER) {
    confirmReceived = false;
    cancelReceived  = false;

    int shipNum = shipsPlaced; // 0-indexed
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

    // Tell ESP32 which ship to prompt for
    char promptBuf[40];
    snprintf(promptBuf, sizeof(promptBuf), "P%d PLACE %s", player, SHIP_NAMES[shipNum]);
    sendPromptToESP32(promptBuf);

    // Read presses
    bool pendingCells[ROWS][COLS] = {};
    int count = readShipPlacement(pins, ships, pendingCells);

    // Cancelled mid-placement
    if (count == -1 || cancelReceived) {
      Serial.println("Placement cancelled, try again.");
      sendPromptToESP32("CANCELLED TRY AGAIN");
      // Clear any pending LEDs
      for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
          if (pendingCells[r][c]) setCellColor(strip, r, c, COLOR_OFF);
      continue;
    }

    // Light up pending cells
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (pendingCells[r][c]) setCellColor(strip, r, c, COLOR_PENDING);

    Serial.print("Cells lit: ");
    Serial.println(count);

    // Tell ESP32 to prompt confirm
    char confirmBuf[40];
    snprintf(confirmBuf, sizeof(confirmBuf), "P%d X=CONFIRM O=CANCEL", player);
    sendPromptToESP32(confirmBuf);
    Serial.println("Press X to confirm or O to cancel...");

    // Wait for confirm or cancel
    while (!confirmReceived && !cancelReceived) {
      checkESP32Serial();
      delay(20);
    }

    if (cancelReceived) {
      // Clear pending LEDs and try again
      for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
          if (pendingCells[r][c]) setCellColor(strip, r, c, COLOR_OFF);
      Serial.println("Cancelled. Place ship again.");
      sendPromptToESP32("CANCELLED TRY AGAIN");
      continue;
    }

    // Confirmed - commit cells
    flashPendingCells(strip, pendingCells, COLOR_CONFIRM, 2);

    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        if (pendingCells[r][c]) {
          ships[r][c]      = true;
          shipIndex[r][c]  = shipNum;
          uint32_t color   = (SHIP_SIZES[shipNum] == 1) ? COLOR_SCOUT : COLOR_DESTROY;
          setCellColor(strip, r, c, color);
        }
      }
    }

    shipsPlaced++;
    sendBoardToESP32(player, ships);
    debugPrintBoard(player == 1 ? "Player 1" : "Player 2", ships);

    Serial.print("Ship confirmed! ");
    Serial.print(SHIPS_PER_PLAYER - shipsPlaced);
    Serial.println(" ships remaining.");
  }

  // All ships placed for this player
  char doneBuf[30];
  snprintf(doneBuf, sizeof(doneBuf), "P%d ALL SHIPS PLACED", player);
  sendPromptToESP32(doneBuf);
  Serial.print("Player ");
  Serial.print(player);
  Serial.println(": All ships placed!");
  debugPrintBoard(player == 1 ? "Player 1" : "Player 2", ships);
}

// ─── Main Placement Phase Entry Point ────────────────────────────────────────

void runPlacementPhase() {

  // Init ship index arrays
  memset(p1ShipIndex, -1, sizeof(p1ShipIndex));
  memset(p2ShipIndex, -1, sizeof(p2ShipIndex));

  Serial.println("\n=== SHIP PLACEMENT PHASE ===");

  // Player 1
  Serial.println("--- PLAYER 1 ---");
  sendPromptToESP32("P1 PLACE YOUR SHIPS");
  placeShipsForPlayer(1, P1_PINS, p1Ships, p1ShipIndex, p1Strip);

  delay(2000); // brief pause between players

  // Player 2
  Serial.println("--- PLAYER 2 ---");
  sendPromptToESP32("P2 PLACE YOUR SHIPS");
  placeShipsForPlayer(2, P2_PINS, p2Ships, p2ShipIndex, p2Strip);

  delay(1000);
  Serial.println("\n=== ALL SHIPS PLACED - GAME STARTING ===");
  sendPromptToESP32("GAME STARTING");
}

#endif
