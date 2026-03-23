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
 *   6. Repeat until all 4 ships placed
 *   7. Player presses Cross again to confirm ready
 *   8. Once both ready, turnPhaseInit() is called and game starts
 *
 * SHIPS (in placement order):
 *   Ship 0 — Cruiser    (3 cells)
 *   Ship 1 — Destroyer  (2 cells)
 *   Ship 2 — Submarine  (2 cells)
 *   Ship 3 — Scout      (1 cell)
 */

#include "setup_phase.h"
#include "turn_phase.h"    // for turnPhaseInit()
#include "game_logic.h"
#include "display.h"
#include "leds.h"

// Track whether each player has been shown their current prompt
static bool promptShown[NUM_PLAYERS] = {false, false};

// ============================================================
// INIT
// ============================================================

void setupPhaseInit() {
  for (int p = 0; p < NUM_PLAYERS; p++) {
    players[p].init();
    promptShown[p] = false;
    displayPlaceShip(p, 0);
    ledsShowShipPlacement(p);
  }
}

// ============================================================
// HANDLE ONE PLAYER'S SETUP LOGIC
// ============================================================

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

    // Clear the undone ship's cells and reset its state
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
      // Read current switch grid state
      updatePlayerGrid(p);

      // Validate the placement
      if (!isValidShipPlacement(p, players[p].shipsPlaced,
                                 players[p].switchGrid)) {
        displayPlacementError(p);
        promptShown[p] = false;
        return;
      }

      // Save ship cells into the ship struct
      extractShipCells(p, players[p].shipsPlaced, players[p].switchGrid);

      Serial.printf("Player %d placed %s at: ", p + 1,
                    SHIP_NAMES[players[p].shipsPlaced]);
      Ship &placed = players[p].ships[players[p].shipsPlaced];
      for (int i = 0; i < placed.size; i++) {
        Serial.printf("(%d,%d) ", placed.cells[i][0], placed.cells[i][1]);
      }
      Serial.println();

      displayShipPlaced(p, players[p].shipsPlaced);
      players[p].shipsPlaced++;
      ledsShowShipPlacement(p);
      promptShown[p] = false;

    } else {
      // All ships placed — player confirms ready
      players[p].ready = true;
      Serial.printf("Player %d is ready.\n", p + 1);
    }
  }
}

// ============================================================
// SETUP PHASE UPDATE — called every loop during PHASE_SETUP
// ============================================================

void setupPhaseUpdate() {
  handlePlayerSetup(0);
  handlePlayerSetup(1);

  // Check if both players are ready
  if (players[0].ready && players[1].ready) {
    displayBothReady();

    // Transition to playing phase
    gamePhase = PHASE_PLAYING;

    // IMPORTANT: initialize turn state before first turnPhaseUpdate() runs
    turnPhaseInit();
  }
}
