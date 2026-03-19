/*
 * game_logic.cpp
 * Core game rules implementation.
 */

#include "game_logic.h"

// ============================================================
// SHIFT REGISTER READS
// ============================================================

uint16_t readGridShiftRegister(int loadPin, int clkPin, int dataPin) {
  uint16_t data = 0;
  digitalWrite(loadPin, LOW);
  delayMicroseconds(5);
  digitalWrite(loadPin, HIGH);
  for (int i = 0; i < 16; i++) {
    data <<= 1;
    if (digitalRead(dataPin) == HIGH) data |= 1;
    digitalWrite(clkPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(clkPin, LOW);
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
    // Beam breaks are active LOW — LOW = beam broken
    if (digitalRead(BEAM_DATA) == LOW) data |= 1;
    digitalWrite(BEAM_CLK, HIGH);
    delayMicroseconds(5);
    digitalWrite(BEAM_CLK, LOW);
    delayMicroseconds(5);
  }
  return data;
}

void updatePlayerGrid(int playerIndex) {
  int loadPin, clkPin, dataPin;
  if (playerIndex == 0) {
    loadPin = P1_GRID_LOAD; clkPin = P1_GRID_CLK; dataPin = P1_GRID_DATA;
  } else {
    loadPin = P2_GRID_LOAD; clkPin = P2_GRID_CLK; dataPin = P2_GRID_DATA;
  }

  uint16_t raw = readGridShiftRegister(loadPin, clkPin, dataPin);
  for (int r = 0; r < GRID_ROWS; r++)
    for (int c = 0; c < GRID_COLS; c++)
      players[playerIndex].switchGrid[r][c] =
        (raw >> (15 - (r * GRID_COLS + c))) & 0x01;
}

// ============================================================
// BEAM BREAK READING
// ============================================================

bool readBeamBreaks(int &row, int &col) {
  /*
   * 8 sensors: bits 7-4 = horizontal beams (rows 0-3)
   *            bits 3-0 = vertical beams   (cols 0-3)
   * A shot registers when exactly one H and one V beam are broken.
   */
  uint8_t raw = readBeamShiftRegister();

  uint8_t hBeams = (raw >> 4) & 0x0F;  // Upper nibble = horizontal
  uint8_t vBeams =  raw       & 0x0F;  // Lower nibble = vertical

  // Count broken beams
  int hCount = __builtin_popcount(hBeams);
  int vCount = __builtin_popcount(vBeams);

  // Valid shot = exactly one H and one V broken simultaneously
  if (hCount != 1 || vCount != 1) return false;

  // Find which row and col
  row = -1; col = -1;
  for (int i = 0; i < 4; i++) {
    if ((hBeams >> (3 - i)) & 0x01) row = i;
    if ((vBeams >> (3 - i)) & 0x01) col = i;
  }

  return (row >= 0 && col >= 0);
}

// ============================================================
// SHIP PLACEMENT VALIDATION
// ============================================================

bool cellAlreadyUsed(int playerIndex, int upToShip, int r, int c) {
  // Check if cell is used by any already-placed ship
  for (int s = 0; s < upToShip; s++) {
    Ship &ship = players[playerIndex].ships[s];
    for (int i = 0; i < ship.size; i++)
      if (ship.cells[i][0] == r && ship.cells[i][1] == c) return true;
  }
  return false;
}

bool isValidShipPlacement(int playerIndex, int shipIndex,
                           bool currentSwitches[GRID_ROWS][GRID_COLS]) {
  int expectedSize = SHIP_SIZES[shipIndex];

  // Collect newly flipped cells (not used by previous ships)
  int newCells[3][2];
  int newCount = 0;

  for (int r = 0; r < GRID_ROWS; r++) {
    for (int c = 0; c < GRID_COLS; c++) {
      if (currentSwitches[r][c] &&
          !cellAlreadyUsed(playerIndex, shipIndex, r, c)) {
        if (newCount >= expectedSize + 1) return false;  // Too many
        newCells[newCount][0] = r;
        newCells[newCount][1] = c;
        newCount++;
      }
    }
  }

  // Must have exactly the right number of new cells
  if (newCount != expectedSize) return false;

  // Must form a straight horizontal or vertical line
  if (expectedSize == 1) return true;  // Single cell always valid

  bool horizontal = true, vertical = true;
  for (int i = 1; i < newCount; i++) {
    if (newCells[i][0] != newCells[0][0]) horizontal = false;
    if (newCells[i][1] != newCells[0][1]) vertical   = false;
  }

  if (!horizontal && !vertical) return false;

  // Must be contiguous (no gaps)
  if (horizontal) {
    // Sort by col and check consecutive
    for (int i = 0; i < newCount - 1; i++)
      for (int j = i + 1; j < newCount; j++)
        if (newCells[j][1] < newCells[i][1]) {
          int tmp = newCells[i][1]; newCells[i][1] = newCells[j][1]; newCells[j][1] = tmp;
        }
    for (int i = 1; i < newCount; i++)
      if (newCells[i][1] != newCells[i-1][1] + 1) return false;
  } else {
    // Sort by row and check consecutive
    for (int i = 0; i < newCount - 1; i++)
      for (int j = i + 1; j < newCount; j++)
        if (newCells[j][0] < newCells[i][0]) {
          int tmp = newCells[i][0]; newCells[i][0] = newCells[j][0]; newCells[j][0] = tmp;
        }
    for (int i = 1; i < newCount; i++)
      if (newCells[i][0] != newCells[i-1][0] + 1) return false;
  }

  return true;
}

bool extractShipCells(int playerIndex, int shipIndex,
                       bool currentSwitches[GRID_ROWS][GRID_COLS]) {
  // Called after validation passes — saves cells into the ship struct
  Ship &ship = players[playerIndex].ships[shipIndex];
  int found = 0;
  for (int r = 0; r < GRID_ROWS && found < ship.size; r++) {
    for (int c = 0; c < GRID_COLS && found < ship.size; c++) {
      if (currentSwitches[r][c] &&
          !cellAlreadyUsed(playerIndex, shipIndex, r, c)) {
        ship.cells[found][0] = r;
        ship.cells[found][1] = c;
        found++;
      }
    }
  }
  return found == ship.size;
}

// ============================================================
// SHOT RESOLUTION
// ============================================================

ShotResult resolveShot(int defendingPlayer, int row, int col) {
  // Check if already shot here
  if (players[defendingPlayer].hitGrid[row][col]) return RESULT_MISS;

  // Mark cell as shot
  players[defendingPlayer].hitGrid[row][col] = true;

  // Check against each ship
  for (int s = 0; s < NUM_SHIPS; s++) {
    Ship &ship = players[defendingPlayer].ships[s];
    if (ship.sunk) continue;

    if (ship.isHit(row, col)) {
      ship.registerHit(row, col);
      lastShotShipIndex = s;
      if (ship.sunk) {
        players[defendingPlayer].shipsRemaining--;
        return RESULT_SUNK;
      }
      return RESULT_HIT;
    }
  }

  lastShotShipIndex = -1;
  return RESULT_MISS;
}

// ============================================================
// WIN CONDITION
// ============================================================

bool checkWin(int playerIndex) {
  return players[playerIndex].shipsRemaining <= 0;
}
