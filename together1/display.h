/*
 * display.h
 * All LCD output functions for both player screens.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "game_state.h"
#include <LiquidCrystal_I2C.h>

// Initialise both LCDs
void displayInit();

// Setup phase
void displayPlaceShip(int playerIndex, int shipIndex);
void displayPlacementError(int playerIndex);
void displayShipPlaced(int playerIndex, int shipIndex);
void displayUndoConfirm(int playerIndex, int shipIndex);
void displayWaitingForReady(int playerIndex);
void displayBothReady();

// Turn phase
void displayYourTurn(int playerIndex);
void displayAiming(int playerIndex);
void displayFireWindow(int playerIndex, int secondsLeft);
void displayTimeOut(int playerIndex);
void displayWaiting(int playerIndex);  // Shown to player NOT currently aiming

// Shot result
void displayMiss(int shooterIndex);
void displayHit(int shooterIndex, int shipIndex);
void displaySunk(int shooterIndex, int shipIndex);
void displayReceivedMiss(int defenderIndex);
void displayReceivedHit(int defenderIndex, int shipIndex);
void displayReceivedSunk(int defenderIndex, int shipIndex);

// Game over
void displayWin(int winnerIndex);
void displayLose(int loserIndex);

// Utilities
void clearDisplay(int playerIndex);
void printCentered(int playerIndex, int row, const char* text);

#endif
