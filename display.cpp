/*
 * display.cpp
 * LCD output implementation for both 20x4 I2C displays.
 */

#include "display.h"

// Two LCD objects, one per player
LiquidCrystal_I2C lcd0(P1_LCD_ADDR, LCD_COLS, LCD_ROWS);
LiquidCrystal_I2C lcd1(P2_LCD_ADDR, LCD_COLS, LCD_ROWS);

// Helper to get the right LCD object
LiquidCrystal_I2C& lcd(int playerIndex) {
  return playerIndex == 0 ? lcd0 : lcd1;
}

// ============================================================
// UTILITIES
// ============================================================

void displayInit() {
  lcd0.init(); lcd0.backlight(); lcd0.clear();
  lcd1.init(); lcd1.backlight(); lcd1.clear();
}

void clearDisplay(int playerIndex) {
  lcd(playerIndex).clear();
}

void printCentered(int playerIndex, int row, const char* text) {
  int len = strlen(text);
  int col = (LCD_COLS - len) / 2;
  if (col < 0) col = 0;
  lcd(playerIndex).setCursor(col, row);
  lcd(playerIndex).print(text);
}

// ============================================================
// SETUP PHASE
// ============================================================

void displayPlaceShip(int playerIndex, int shipIndex) {
  lcd(playerIndex).clear();
  printCentered(playerIndex, 0, playerIndex == 0 ? "=== PLAYER 1 ===" : "=== PLAYER 2 ===");

  char line[21];
  snprintf(line, sizeof(line), "Place %s", SHIP_NAMES[shipIndex]);
  printCentered(playerIndex, 1, line);

  snprintf(line, sizeof(line), "Size: %d cell%s",
           SHIP_SIZES[shipIndex], SHIP_SIZES[shipIndex] > 1 ? "s" : "");
  printCentered(playerIndex, 2, line);

  printCentered(playerIndex, 3, "X=Confirm O=Undo");
}

void displayPlacementError(int playerIndex) {
  lcd(playerIndex).clear();
  printCentered(playerIndex, 0, "!! INVALID !!");
  printCentered(playerIndex, 1, "Must be straight");
  printCentered(playerIndex, 2, "line, correct size");
  printCentered(playerIndex, 3, "Try again...");
  delay(2000);
}

void displayShipPlaced(int playerIndex, int shipIndex) {
  lcd(playerIndex).clear();
  char line[21];
  snprintf(line, sizeof(line), "%s placed!", SHIP_NAMES[shipIndex]);
  printCentered(playerIndex, 0, line);

  if (shipIndex + 1 < NUM_SHIPS) {
    snprintf(line, sizeof(line), "Next: %s", SHIP_NAMES[shipIndex + 1]);
    printCentered(playerIndex, 2, line);
    snprintf(line, sizeof(line), "(size %d)", SHIP_SIZES[shipIndex + 1]);
    printCentered(playerIndex, 3, line);
  }
  delay(1500);
}

void displayUndoConfirm(int playerIndex, int shipIndex) {
  lcd(playerIndex).clear();
  char line[21];
  snprintf(line, sizeof(line), "%s removed.", SHIP_NAMES[shipIndex]);
  printCentered(playerIndex, 0, line);
  printCentered(playerIndex, 1, "Flip switches back");
  printCentered(playerIndex, 2, "then re-place.");
  snprintf(line, sizeof(line), "Now placing: %s", SHIP_NAMES[shipIndex]);
  printCentered(playerIndex, 3, line);
  delay(1500);
}

void displayWaitingForReady(int playerIndex) {
  lcd(playerIndex).clear();
  printCentered(playerIndex, 0, "All ships placed!");
  printCentered(playerIndex, 1, "Press X when");
  printCentered(playerIndex, 2, "ready to play.");
  printCentered(playerIndex, 3, "Waiting...");
}

void displayBothReady() {
  for (int p = 0; p < NUM_PLAYERS; p++) {
    lcd(p).clear();
    printCentered(p, 0, "=== BATTLE ===");
    printCentered(p, 1, "Both players ready!");
    printCentered(p, 2, "Game starting...");
  }
  delay(2000);
}

// ============================================================
// TURN PHASE
// ============================================================

void displayYourTurn(int playerIndex) {
  lcd(playerIndex).clear();
  printCentered(playerIndex, 0, "=== YOUR TURN ===");
  printCentered(playerIndex, 1, "Aim your launcher");
  printCentered(playerIndex, 2, "Stop to start");
  printCentered(playerIndex, 3, "fire window.");
}

void displayAiming(int playerIndex) {
  lcd(playerIndex).clear();
  printCentered(playerIndex, 0, "=== YOUR TURN ===");
  printCentered(playerIndex, 1, "Aiming...");
  printCentered(playerIndex, 2, "Stop moving to");
  printCentered(playerIndex, 3, "start fire window.");
}

void displayFireWindow(int playerIndex, int secondsLeft) {
  lcd(playerIndex).clear();
  printCentered(playerIndex, 0, "=== FIRE! ===");
  printCentered(playerIndex, 1, "Launch the ball!");
  char line[21];
  snprintf(line, sizeof(line), "Time: %d second%s",
           secondsLeft, secondsLeft == 1 ? "" : "s");
  printCentered(playerIndex, 2, line);
  printCentered(playerIndex, 3, "Move to re-aim.");
}

void displayTimeOut(int playerIndex) {
  lcd(playerIndex).clear();
  printCentered(playerIndex, 0, "=== TIME OUT ===");
  printCentered(playerIndex, 1, "Turn passed.");
  delay(LCD_MESSAGE_DURATION);
}

void displayWaiting(int playerIndex) {
  lcd(playerIndex).clear();
  printCentered(playerIndex, 0, "Opponent's turn.");
  printCentered(playerIndex, 1, "Defend your grid!");
}

// ============================================================
// SHOT RESULT — SHOOTER'S LCD
// ============================================================

void displayMiss(int shooterIndex) {
  lcd(shooterIndex).clear();
  printCentered(shooterIndex, 0, "================");
  printCentered(shooterIndex, 1, "     MISS!      ");
  printCentered(shooterIndex, 2, "================");
  printCentered(shooterIndex, 3, "No ship there.");
}

void displayHit(int shooterIndex, int shipIndex) {
  lcd(shooterIndex).clear();
  printCentered(shooterIndex, 0, "================");
  printCentered(shooterIndex, 1, "      HIT!      ");
  printCentered(shooterIndex, 2, "================");
  char line[21];
  snprintf(line, sizeof(line), "Hit their %s!", SHIP_NAMES[shipIndex]);
  printCentered(shooterIndex, 3, line);
}

void displaySunk(int shooterIndex, int shipIndex) {
  lcd(shooterIndex).clear();
  printCentered(shooterIndex, 0, "================");
  printCentered(shooterIndex, 1, "  HIT AND SUNK! ");
  printCentered(shooterIndex, 2, "================");
  char line[21];
  snprintf(line, sizeof(line), "Sunk their %s!", SHIP_NAMES[shipIndex]);
  printCentered(shooterIndex, 3, line);
}

// ============================================================
// SHOT RESULT — DEFENDER'S LCD
// ============================================================

void displayReceivedMiss(int defenderIndex) {
  lcd(defenderIndex).clear();
  printCentered(defenderIndex, 0, "Opponent fired!");
  printCentered(defenderIndex, 1, "----------------");
  printCentered(defenderIndex, 2, "     MISS!      ");
  printCentered(defenderIndex, 3, "Your ships safe.");
}

void displayReceivedHit(int defenderIndex, int shipIndex) {
  lcd(defenderIndex).clear();
  printCentered(defenderIndex, 0, "Opponent fired!");
  printCentered(defenderIndex, 1, "----------------");
  printCentered(defenderIndex, 2, "     HIT!       ");
  char line[21];
  snprintf(line, sizeof(line), "%s damaged!", SHIP_NAMES[shipIndex]);
  printCentered(defenderIndex, 3, line);
}

void displayReceivedSunk(int defenderIndex, int shipIndex) {
  lcd(defenderIndex).clear();
  printCentered(defenderIndex, 0, "Opponent fired!");
  printCentered(defenderIndex, 1, "----------------");
  printCentered(defenderIndex, 2, "  SHIP SUNK!    ");
  char line[21];
  snprintf(line, sizeof(line), "%s lost!", SHIP_NAMES[shipIndex]);
  printCentered(defenderIndex, 3, line);
}

// ============================================================
// GAME OVER
// ============================================================

void displayWin(int winnerIndex) {
  lcd(winnerIndex).clear();
  printCentered(winnerIndex, 0, "****************");
  printCentered(winnerIndex, 1, "   YOU WIN!!    ");
  printCentered(winnerIndex, 2, "****************");
  printCentered(winnerIndex, 3, "All ships sunk!");
}

void displayLose(int loserIndex) {
  lcd(loserIndex).clear();
  printCentered(loserIndex, 0, "****************");
  printCentered(loserIndex, 1, "  GAME OVER...  ");
  printCentered(loserIndex, 2, "****************");
  printCentered(loserIndex, 3, "All your ships sunk");
}
