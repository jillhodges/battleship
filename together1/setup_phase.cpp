/*
 * setup_phase.cpp
 * Ship placement phase implementation.
 *
 * FLOW PER PLAYER (runs simultaneously for both):
 *   1. LCD prompts to place current ship
 *   2. Player flips switches on their grid
 *   3. Player presses Cross to confirm placement
 *   4. System validates — if invalid, shows error and waits
 *   5. Player presses Circle to undo last ship (goes back one)
 *   6. Repeat until all ships placed
 *   7. Player presses Cross to confirm ready
 *   8. Once both ready, game starts
 */

#include "setup_phase.h"
#include "game_logic.h"
#include "display.h"
#include "leds.h"

// Track whether each player has been shown their current prompt
static bool promptShown[NUM_PLAYERS] = {false, false};

void setupPhaseInit() {
  for (int p = 0; p < NUM_PLAYERS; p++) {
    players[p].init();
    promptShown[p] = false;
    displayPlaceShip(p, 0);
    ledsShowShipPlacement(p);
  }
}

// ---- Handle one player's setup logic ----
void handlePlayerSetup(int p) {
  // Already ready — nothing to do
  if (players[p].ready) return;

  // Show prompt if not yet shown for current ship
  if (!promptShown[p]) {
    if (players[p].shipsPlaced < NUM_SHIPS) {
      displayPlaceShip(p, players[p].shipsPlaced);
    } else {
      displayWaitingForReady(p);
    }
    promptShown[p] = true;
  }

  // --- UNDO: Circle button — only if at least one ship placed ---
  if (ctrl[p].circlePressed && players[p].shipsPlaced > 0) {
    players[p].shipsPlaced--;

    // Clear the undone ship's cells
    Ship &undone = players[p].ships[players[p].shipsPlaced];
    for (int i = 0; i < undone.size; i++) {
      undone.cells[i][0] = -1;
      undone.cells[i][1] = -1;
    }
    undone.hits = 0;
    undone.sunk = false;

    displayUndoConfirm(p, players[p].shipsPlaced);
    ledsShowShipPlacement(p);
    promptShown[p] = false;
    return;
  }

  // --- CONFIRM: Cross button ---
  if (ctrl[p].crossPressed) {

    if (players[p].shipsPlaced < NUM_SHIPS) {
      // Read current switch grid
      updatePlayerGrid(p);

      // Validate placement
      if (!isValidShipPlacement(p, players[p].shipsPlaced,
                                 players[p].switchGrid)) {
        displayPlacementError(p);
        promptShown[p] = false;
        return;
      }

      // Save ship cells
      extractShipCells(p, players[p].shipsPlaced, players[p].switchGrid);
      displayShipPlaced(p, players[p].shipsPlaced);
      players[p].shipsPlaced++;
      ledsShowShipPlacement(p);
      promptShown[p] = false;

    } else {
      // All ships placed — confirm ready
      players[p].ready = true;
      Serial.printf("Player %d is ready.\n", p + 1);
    }
  }
}

void setupPhaseUpdate() {
  handlePlayerSetup(0);
  handlePlayerSetup(1);

  // Check if both players are ready
  if (players[0].ready && players[1].ready) {
    displayBothReady();
    gamePhase = PHASE_PLAYING;
    currentPlayer = 0;
    turnStartTime = millis();
    playerStoppedAiming = false;
  }
}
