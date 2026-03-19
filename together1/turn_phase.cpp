/*
 * turn_phase.cpp
 * Turn phase implementation.
 *
 * TURN FLOW:
 *   1. Current player's LCD shows "Your turn — aim"
 *   2. Waiting player's LCD shows "Opponent's turn"
 *   3. While joystick is moving: show aiming message, reset fire window
 *   4. When joystick stops: start 3 second fire window, countdown on LCD
 *   5. If beam break detected in fire window: register shot, show result
 *   6. If joystick moves again in fire window: reset window, back to aiming
 *   7. If 30 second hard cap expires: turn passes, timeout message
 *   8. After shot result displayed: pass turn to other player
 */

#include "turn_phase.h"
#include "game_logic.h"
#include "display.h"
#include "leds.h"

// Internal state
static bool     inFireWindow    = false;
static bool     promptShown     = false;
static int      lastSecondsLeft = -1;

// ---- Start a new turn ----
void turnPhaseInit() {
  currentPlayer       = 0;
  turnStartTime       = millis();
  aimStopTime         = 0;
  playerStoppedAiming = false;
  inFireWindow        = false;
  promptShown         = false;
  lastSecondsLeft     = -1;

  displayYourTurn(currentPlayer);
  displayWaiting(1 - currentPlayer);
}

// ---- Pass turn to the other player ----
void passTurn() {
  currentPlayer       = 1 - currentPlayer;
  turnStartTime       = millis();
  aimStopTime         = 0;
  playerStoppedAiming = false;
  inFireWindow        = false;
  promptShown         = false;
  lastSecondsLeft     = -1;

  displayYourTurn(currentPlayer);
  displayWaiting(1 - currentPlayer);
}

// ---- Handle shot result display and LED updates ----
void handleShotResult(int shooterIndex, int row, int col) {
  int defenderIndex = 1 - shooterIndex;
  ShotResult result = resolveShot(defenderIndex, row, col);
  lastShotResult = result;
  lastShotRow    = row;
  lastShotCol    = col;

  switch (result) {
    case RESULT_MISS:
      displayMiss(shooterIndex);
      displayReceivedMiss(defenderIndex);
      ledsShowMiss(shooterIndex, row, col);
      break;

    case RESULT_HIT:
      displayHit(shooterIndex, lastShotShipIndex);
      displayReceivedHit(defenderIndex, lastShotShipIndex);
      ledsShowHit(shooterIndex, row, col);
      break;

    case RESULT_SUNK:
      displaySunk(shooterIndex, lastShotShipIndex);
      displayReceivedSunk(defenderIndex, lastShotShipIndex);
      ledsShowHit(shooterIndex, row, col);

      // Check win condition
      if (checkWin(defenderIndex)) {
        delay(LCD_MESSAGE_DURATION);
        displayWin(shooterIndex);
        displayLose(defenderIndex);
        ledsShowWin(shooterIndex);
        gamePhase = PHASE_GAME_OVER;
        return;
      }
      break;

    default:
      break;
  }

  // Show result for a moment then pass turn
  delay(LCD_MESSAGE_DURATION);
  passTurn();
}

// ---- Main turn update — called every loop ----
void turnPhaseUpdate() {
  unsigned long now        = millis();
  unsigned long elapsed    = now - turnStartTime;
  ControllerState &shooter = ctrl[currentPlayer];

  // --- Hard cap timeout ---
  if (elapsed >= TURN_HARD_CAP_MS) {
    displayTimeOut(currentPlayer);
    passTurn();
    return;
  }

  // --- Show initial prompt once ---
  if (!promptShown) {
    displayYourTurn(currentPlayer);
    displayWaiting(1 - currentPlayer);
    promptShown = true;
  }

  bool currentlyAiming = shooter.isAiming();

  // --- Joystick is moving ---
  if (currentlyAiming) {
    if (inFireWindow || playerStoppedAiming) {
      // Player re-aimed — reset fire window
      inFireWindow        = false;
      playerStoppedAiming = false;
      aimStopTime         = 0;
      lastSecondsLeft     = -1;
      displayAiming(currentPlayer);
    } else if (!promptShown) {
      displayAiming(currentPlayer);
    }
  }

  // --- Joystick stopped ---
  if (!currentlyAiming) {
    if (!playerStoppedAiming) {
      // Just stopped — start fire window
      playerStoppedAiming = true;
      aimStopTime         = now;
      inFireWindow        = true;
      lastSecondsLeft     = -1;
    }

    if (inFireWindow) {
      unsigned long windowElapsed = now - aimStopTime;

      if (windowElapsed >= FIRE_WINDOW_MS) {
        // Fire window expired — pass turn
        displayTimeOut(currentPlayer);
        passTurn();
        return;
      }

      // Update countdown display only when seconds change
      int secondsLeft = (int)((FIRE_WINDOW_MS - windowElapsed) / 1000) + 1;
      if (secondsLeft != lastSecondsLeft) {
        lastSecondsLeft = secondsLeft;
        displayFireWindow(currentPlayer, secondsLeft);
      }

      // --- Check for beam break (shot fired) ---
      int shotRow, shotCol;
      if (readBeamBreaks(shotRow, shotCol)) {
        handleShotResult(currentPlayer, shotRow, shotCol);
        return;
      }
    }
  }
}
