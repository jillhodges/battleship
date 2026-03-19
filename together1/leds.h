/*
 * leds.h
 * NeoPixel LED strip control for all four strips.
 */

#ifndef LEDS_H
#define LEDS_H

#include "game_state.h"
#include <Adafruit_NeoPixel.h>

void ledsInit();
void ledsShowHit(int shooterIndex, int row, int col);
void ledsShowMiss(int shooterIndex, int row, int col);
void ledsShowShipPlacement(int playerIndex);
void ledsShowWin(int winnerIndex);
void ledsClear(int stripIndex);
void ledsAllOff();

#endif
