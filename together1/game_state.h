/*
 * game_state.h
 * Shared data structures, constants, and global game state.
 * Include this in every other file.
 */

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <Arduino.h>

// ============================================================
// GRID CONSTANTS
// ============================================================

#define GRID_ROWS       4
#define GRID_COLS       4
#define NUM_SHIPS       4       // Ships per player: 3-cell, 2-cell, 2-cell, 1-cell
#define NUM_BEAM_H      4       // Horizontal beam break sensors
#define NUM_BEAM_V      4       // Vertical beam break sensors
#define NUM_PLAYERS     2

// ============================================================
// SHIP DEFINITIONS
// ============================================================

// IMPORTANT: static prevents "multiple definition" linker errors
// when this header is included by multiple .cpp files
static const int   SHIP_SIZES[NUM_SHIPS] = {3, 2, 2, 1};
static const char* SHIP_NAMES[NUM_SHIPS] = {"Cruiser", "Destroyer", "Submarine", "Scout"};

// ============================================================
// TIMING
// ============================================================

#define TURN_HARD_CAP_MS      30000   // 30 second hard cap per turn
#define FIRE_WINDOW_MS         3000   // 3 seconds after aiming stops
#define JOYSTICK_DEADZONE        15   // Axis deadzone (0-255 range from UART)
#define LCD_MESSAGE_DURATION   2000   // How long hit/miss message shows (ms)

// ============================================================
// SHIFT REGISTER PINS
// ============================================================

// Player 1 switch grid (74HC165)
#define P1_GRID_LOAD    8
#define P1_GRID_CLK     9
#define P1_GRID_DATA    11

// Player 2 switch grid (74HC165)
#define P2_GRID_LOAD    2
#define P2_GRID_CLK     3
#define P2_GRID_DATA    4

// Beam break sensors (74HC165)
#define BEAM_LOAD       5
#define BEAM_CLK        6
#define BEAM_DATA       7

// ============================================================
// LED STRIP PINS + CONFIG
// ============================================================

#define P1_SHIP_LED_PIN     22    // Player 1 ship grid LED strip
#define P1_SHOOT_LED_PIN    23    // Player 1 shooting grid LED strip
#define P2_SHIP_LED_PIN     24    // Player 2 ship grid LED strip
#define P2_SHOOT_LED_PIN    25    // Player 2 shooting grid LED strip
#define LEDS_PER_STRIP      16    // 4x4 grid = 16 LEDs per strip

// Colors (GRB format for NeoPixel)
#define COLOR_HIT       0xFF0000   // Red
#define COLOR_MISS      0x0000FF   // Blue
#define COLOR_OFF       0x000000   // Off
#define COLOR_SHIP      0x00FF00   // Green — ship placement indicator
#define COLOR_READY     0xFFFF00   // Yellow — ready up
#define COLOR_WIN       0xFF00FF   // Purple — win screen

// ============================================================
// I2C LCD ADDRESSES
// ============================================================

#define P1_LCD_ADDR     0x27      // Player 1 LCD I2C address
#define P2_LCD_ADDR     0x26      // Player 2 LCD I2C address
#define LCD_COLS        20
#define LCD_ROWS        4

// ============================================================
// UART TO ESP32
// ============================================================

#define SERIAL_ESP      Serial1
#define BAUD_RATE       115200

// ============================================================
// GAME PHASES
// ============================================================

enum GamePhase {
  PHASE_SETUP,        // Ship placement
  PHASE_READY,        // Both players confirmed, countdown to start
  PHASE_PLAYING,      // Main game loop
  PHASE_SHOT_RESULT,  // Displaying hit/miss/sunk result
  PHASE_GAME_OVER     // End screen
};

// ============================================================
// SHIP STRUCT
// ============================================================

struct Ship {
  int  cells[3][2];   // Max 3 cells — matches largest ship (Cruiser, size 3)
                      // Each cell is {row, col}
  int  size;          // Number of cells this ship occupies
  int  hits;          // How many cells have been hit so far
  bool sunk;          // True when hits >= size

  // Returns true if this ship occupies cell (r, c)
  bool isHit(int r, int c) {
    for (int i = 0; i < size; i++)
      if (cells[i][0] == r && cells[i][1] == c) return true;
    return false;
  }

  // Register a hit on cell (r, c) — sets sunk if fully destroyed
  void registerHit(int r, int c) {
    for (int i = 0; i < size; i++) {
      if (cells[i][0] == r && cells[i][1] == c) {
        hits++;
        if (hits >= size) sunk = true;
        return;
      }
    }
  }
};

// ============================================================
// PLAYER STRUCT
// ============================================================

struct Player {
  Ship  ships[NUM_SHIPS];
  bool  switchGrid[GRID_ROWS][GRID_COLS];   // Raw switch state from shift registers
  bool  hitGrid[GRID_ROWS][GRID_COLS];      // Cells that have been shot at
  int   shipsPlaced;                         // How many ships confirmed so far
  bool  ready;                               // Player confirmed ready to play
  int   shipsRemaining;                      // Ships not yet sunk

  void init() {
    for (int i = 0; i < NUM_SHIPS; i++) {
      ships[i].size = SHIP_SIZES[i];
      ships[i].hits = 0;
      ships[i].sunk = false;
      for (int j = 0; j < 3; j++) {
        ships[i].cells[j][0] = -1;
        ships[i].cells[j][1] = -1;
      }
    }
    for (int r = 0; r < GRID_ROWS; r++)
      for (int c = 0; c < GRID_COLS; c++) {
        switchGrid[r][c] = false;
        hitGrid[r][c]    = false;
      }
    shipsPlaced    = 0;
    ready          = false;
    shipsRemaining = NUM_SHIPS;   // 4 ships per player
  }
};

// ============================================================
// CONTROLLER STATE (received from ESP32 via UART)
// ============================================================

struct ControllerState {
  bool connected;
  int  lx;          // -511 to +511
  int  ly;

  // Current button states
  bool cross, circle, square, triangle;
  bool l1, r1, l2, r2;

  // Edge detection — true only on the single frame a button is first pressed
  bool crossPressed, circlePressed, squarePressed, trianglePressed;
  bool l1Pressed, r1Pressed;

  // Previous button states (used internally for edge detection)
  bool _prevCross, _prevCircle, _prevSquare, _prevTriangle;
  bool _prevL1, _prevR1;

  // Call once per loop AFTER receiving new data from ESP32
  void updateEdges() {
    crossPressed    = cross    && !_prevCross;
    circlePressed   = circle   && !_prevCircle;
    squarePressed   = square   && !_prevSquare;
    trianglePressed = triangle && !_prevTriangle;
    l1Pressed       = l1       && !_prevL1;
    r1Pressed       = r1       && !_prevR1;

    _prevCross    = cross;
    _prevCircle   = circle;
    _prevSquare   = square;
    _prevTriangle = triangle;
    _prevL1       = l1;
    _prevR1       = r1;
  }

  // Returns true if joystick is outside deadzone (player is actively aiming)
  bool isAiming() {
    return abs(lx) > JOYSTICK_DEADZONE || abs(ly) > JOYSTICK_DEADZONE;
  }
};

// ============================================================
// SHOT RESULT
// ============================================================

enum ShotResult {
  RESULT_NONE,
  RESULT_MISS,
  RESULT_HIT,
  RESULT_SUNK
};

// ============================================================
// GLOBAL GAME STATE (defined in battleship_mega.ino)
// ============================================================

extern GamePhase       gamePhase;
extern Player          players[NUM_PLAYERS];
extern ControllerState ctrl[NUM_PLAYERS];
extern int             currentPlayer;        // 0 or 1 — whose turn it is
extern unsigned long   turnStartTime;        // When current turn began
extern unsigned long   aimStopTime;          // When player stopped aiming
extern bool            playerStoppedAiming;  // Flag for fire window
extern ShotResult      lastShotResult;
extern int             lastShotRow;
extern int             lastShotCol;
extern int             lastShotShipIndex;    // Which ship was hit (-1 if miss)

#endif
