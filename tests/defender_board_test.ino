/*
 * 4x4 Microswitch Grid - Arduino Mega
 *
 * Pin mapping (all INPUT_PULLUP, switches connect pin to GND):
 *   Row 0: pins 22, 23, 24, 25
 *   Row 1: pins 26, 27, 28, 29
 *   Row 2: pins 30, 31, 32, 33
 *   Row 3: pins 34, 35, 36, 37
 *
 * Serial output refreshes whenever a switch state changes.
 * Occupied cells show [X], empty cells show [ ].
 */

const int ROWS = 4;
const int COLS = 4;

// Pin layout: switchPins[row][col]
const int switchPins[ROWS][COLS] = {
  {22, 23, 24, 25},  // Row 0
  {26, 27, 28, 29},  // Row 1
  {30, 31, 32, 33},  // Row 2
  {34, 35, 36, 37}   // Row 3
};

bool occupied[ROWS][COLS] = {};   // current state
bool prevState[ROWS][COLS] = {};  // previous state for change detection

void setup() {
  Serial.begin(9600);

  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      pinMode(switchPins[r][c], INPUT_PULLUP);
      occupied[r][c] = false;
      prevState[r][c] = false;
    }
  }

  Serial.println("=== 4x4 Microswitch Grid ===");
  Serial.println("Waiting for input...\n");
}

void loop() {
  bool changed = false;

  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      // LOW = pressed (INPUT_PULLUP: switch pulls pin to GND when closed)
      bool pressed = (digitalRead(switchPins[r][c]) == HIGH);

      if (pressed != prevState[r][c]) {
        occupied[r][c] = pressed;
        prevState[r][c] = pressed;
        changed = true;
      }
    }
  }

  if (changed) {
    printGrid();
  }

  delay(20);  // simple debounce
}

void printGrid() {
  Serial.println("+---------+---------+---------+---------+");
  Serial.println("| Col  0  | Col  1  | Col  2  | Col  3  |");
  Serial.println("+---------+---------+---------+---------+");

  for (int r = 0; r < ROWS; r++) {
    Serial.print("| ");
    for (int c = 0; c < COLS; c++) {
      if (occupied[r][c]) {
        Serial.print("  [X]    | ");
      } else {
        Serial.print("  [ ]    | ");
      }
    }
    Serial.print("  <- Row ");
    Serial.println(r);
    Serial.println("+---------+---------+---------+---------+");
  }

  // Summary of occupied cells
  Serial.print("\nOccupied: ");
  bool any = false;
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      if (occupied[r][c]) {
        if (any) Serial.print(", ");
        Serial.print("R");
        Serial.print(r);
        Serial.print("C");
        Serial.print(c);
        any = true;
      }
    }
  }
  if (!any) Serial.print("none");
  Serial.println("\n");
}
