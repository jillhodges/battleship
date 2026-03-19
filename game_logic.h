/*
 * game_logic.h
 * Core game rules — ship validation, hit detection, win condition.
 */

#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "game_state.h"

// Ship placement validation
bool isValidShipPlacement(int playerIndex, int shipIndex, bool currentSwitches[GRID_ROWS][GRID_COLS]);
bool extractShipCells(int playerIndex, int shipIndex, bool currentSwitches[GRID_ROWS][GRID_COLS]);
bool cellAlreadyUsed(int playerIndex, int shipIndex, int r, int c);

// Beam break reading
bool readBeamBreaks(int &row, int &col);

// Shot resolution
ShotResult resolveShot(int defendingPlayer, int row, int col);

// Win condition
bool checkWin(int playerIndex);

// Shift register reads
uint16_t readGridShiftRegister(int loadPin, int clkPin, int dataPin);
uint8_t  readBeamShiftRegister();
void     updatePlayerGrid(int playerIndex);

#endif
