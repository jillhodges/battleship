/*
 * leds.cpp
 * NeoPixel LED strip implementation.
 * Four strips total — ship grid and shoot grid for each player.
 *
 * Strip index reference:
 *   0 = Player 1 ship grid   (P1_SHIP_LED_PIN)
 *   1 = Player 1 shoot grid  (P1_SHOOT_LED_PIN)
 *   2 = Player 2 ship grid   (P2_SHIP_LED_PIN)
 *   3 = Player 2 shoot grid  (P2_SHOOT_LED_PIN)
 *
 * LED index within strip:
 *   LED index = row * GRID_COLS + col
 */

#include "leds.h"

Adafruit_NeoPixel strips[4] = {
  Adafruit_NeoPixel(LEDS_PER_STRIP, P1_SHIP_LED_PIN,  NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(LEDS_PER_STRIP, P1_SHOOT_LED_PIN, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(LEDS_PER_STRIP, P2_SHIP_LED_PIN,  NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(LEDS_PER_STRIP, P2_SHOOT_LED_PIN, NEO_GRB + NEO_KHZ800)
};

// ============================================================
// HELPERS
// ============================================================

int ledIndex(int row, int col) {
  return row * GRID_COLS + col;
}

void setLED(int stripIndex, int row, int col, uint32_t color) {
  strips[stripIndex].setPixelColor(ledIndex(row, col), color);
  strips[stripIndex].show();
}

// ============================================================
// INIT
// ============================================================

void ledsInit() {
  for (int i = 0; i < 4; i++) {
    strips[i].begin();
    strips[i].setBrightness(80);  // 0-255, keep reasonable for power draw
    strips[i].clear();
    strips[i].show();
  }
}

// ============================================================
// GAMEPLAY
// ============================================================

void ledsShowHit(int shooterIndex, int row, int col) {
  /*
   * A hit lights up red on:
   *   - The defender's ship grid (their ship is damaged)
   *   - The shooter's shoot grid (they scored a hit)
   */
  int defenderIndex = 1 - shooterIndex;

  // Defender ship grid
  int defenderShipStrip = defenderIndex == 0 ? 0 : 2;
  setLED(defenderShipStrip, row, col, COLOR_HIT);

  // Shooter shoot grid
  int shooterShootStrip = shooterIndex == 0 ? 1 : 3;
  setLED(shooterShootStrip, row, col, COLOR_HIT);
}

void ledsShowMiss(int shooterIndex, int row, int col) {
  /*
   * A miss lights up blue on:
   *   - The shooter's shoot grid (they missed)
   *   - The defender's ship grid stays unchanged (no ship there)
   */
  int shooterShootStrip = shooterIndex == 0 ? 1 : 3;
  setLED(shooterShootStrip, row, col, COLOR_MISS);
}

// ============================================================
// SETUP PHASE — show current ship placement
// ============================================================

void ledsShowShipPlacement(int playerIndex) {
  int shipStrip = playerIndex == 0 ? 0 : 2;
  strips[shipStrip].clear();

  for (int s = 0; s < players[playerIndex].shipsPlaced; s++) {
    Ship &ship = players[playerIndex].ships[s];
    for (int i = 0; i < ship.size; i++) {
      int r = ship.cells[i][0];
      int c = ship.cells[i][1];
      if (r >= 0 && c >= 0)
        strips[shipStrip].setPixelColor(ledIndex(r, c), COLOR_SHIP);
    }
  }
  strips[shipStrip].show();
}

// ============================================================
// GAME OVER — winning player's strips flash
// ============================================================

void ledsShowWin(int winnerIndex) {
  int shipStrip  = winnerIndex == 0 ? 0 : 2;
  int shootStrip = winnerIndex == 0 ? 1 : 3;

  for (int flash = 0; flash < 6; flash++) {
    uint32_t color = (flash % 2 == 0) ? COLOR_WIN : COLOR_OFF;
    for (int i = 0; i < LEDS_PER_STRIP; i++) {
      strips[shipStrip].setPixelColor(i, color);
      strips[shootStrip].setPixelColor(i, color);
    }
    strips[shipStrip].show();
    strips[shootStrip].show();
    delay(400);
  }
}

// ============================================================
// UTILITIES
// ============================================================

void ledsClear(int stripIndex) {
  strips[stripIndex].clear();
  strips[stripIndex].show();
}

void ledsAllOff() {
  for (int i = 0; i < 4; i++) {
    strips[i].clear();
    strips[i].show();
  }
}
