/*
 * defender_board_test.ino
 * 
 * Tests ONE player's 4x4 defender board via 2x daisy-chained 74HC165 shift registers.
 * 
 * PURPOSE:
 *   Standalone test — prints the 4x4 occupancy grid to Serial as you
 *   physically place and remove switches/ships. Verify all 16 cells
 *   read correctly before integrating into DefenderBoard.cpp.
 *
 * NOTE:
 *   This tests ONE player's board (16 switches, 2 chips).
 *   Full game uses TWO boards (32 switches, 4 chips).
 *   See DefenderBoard.cpp for the full dual-board implementation.
 *
 * ---------------------------------------------------------------
 * WIRING (2x 74HC165, daisy-chained)
 * ---------------------------------------------------------------
 *
 *   Arduino Mega Pin 8  --> SH/!LD  (Pin 1  on BOTH chips, tied together)
 *   Arduino Mega Pin 9  --> CLK     (Pin 2  on BOTH chips, tied together)
 *   Arduino Mega Pin 10 --> CLK INH (Pin 15 on BOTH chips, tie to GND)
 *   Arduino Mega Pin 11 --> QH of Chip 2 (last chip in chain, Pin 9)
 *
 *   Daisy-chain:
 *     Chip 1 QH (Pin 9)  --> Chip 2 SER (Pin 10)
 *     Chip 1 SER (Pin 10) --> GND
 *
 *   Per switch (active HIGH):
 *     Switch side A --> Chip parallel input pin (A-H)
 *     Switch side B --> VCC (5V)
 *     10k pull-down resistor from input pin to GND
 *     Switch ON (closed) = HIGH = cell occupied
 *
 * ---------------------------------------------------------------
 * BIT-TO-CELL MAPPING
 * ---------------------------------------------------------------
 *
 *   Bit 15 (first clocked out, Chip 1 input A) = grid[0][0]
 *   Bit 14 = grid[0][1]
 *   Bit 13 = grid[0][2]
 *   Bit 12 = grid[0][3]
 *   Bit 11 = grid[1][0]
 *   ...
 *   Bit 0  (last clocked out, Chip 2 input H) = grid[3][3]
 *
 *   Formula: bitIndex = 15 - (row * COLS + col)
 */

// ---------------------------------------------------------------
// Pin definitions
// ---------------------------------------------------------------
#define LOAD_PIN   8    // SH/!LD: pulse LOW to latch parallel inputs
#define CLOCK_PIN  9    // Shift clock
#define DATA_PIN   11   // Serial data in from last chip QH
#define CLKINH_PIN 10   // CLK INH: tie LOW to enable (can also hard-wire to GND)

// ---------------------------------------------------------------
// Grid dimensions
// ---------------------------------------------------------------
#define ROWS      4
#define COLS      4
#define NUM_CHIPS 2     // 2 chips x 8 bits = 16 inputs for one player board

// ---------------------------------------------------------------
// Grid state
// ---------------------------------------------------------------
bool grid[ROWS][COLS];
bool prevGrid[ROWS][COLS];  // track changes so we only print on update

// ---------------------------------------------------------------
// readShiftRegisters()
// Latches all parallel inputs then clocks out 16 bits MSB-first.
// Returns raw 16-bit value.
// ---------------------------------------------------------------
uint16_t readShiftRegisters() {
  uint16_t data = 0;

  // Pulse LOAD low to latch all parallel inputs simultaneously
  digitalWrite(LOAD_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(LOAD_PIN, HIGH);
  delayMicroseconds(5);

  // Clock out 16 bits (2 chips x 8 bits), MSB first
  for (int i = 0; i < 16; i++) {
    data <<= 1;
    if (digitalRead(DATA_PIN) == HIGH) {
      data |= 1;
    }
    // Pulse clock to advance shift register
    digitalWrite(CLOCK_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(CLOCK_PIN, LOW);
    delayMicroseconds(5);
  }

  return data;
}

// ---------------------------------------------------------------
// updateGrid()
// Maps 16-bit raw data to 4x4 bool grid.
// Bit 15 = grid[0][0], Bit 0 = grid[3][3].
// ---------------------------------------------------------------
void updateGrid(uint16_t rawData) {
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      int bitIndex = 15 - (r * COLS + c);
      grid[r][c] = (rawData >> bitIndex) & 0x01;  // 1 = occupied
    }
  }
}

// ---------------------------------------------------------------
// gridChanged()
// Returns true if grid state differs from previous read.
// ---------------------------------------------------------------
bool gridChanged() {
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      if (grid[r][c] != prevGrid[r][c]) return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------
// savePrevGrid()
// Copies current grid into prevGrid for change detection.
// ---------------------------------------------------------------
void savePrevGrid() {
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      prevGrid[r][c] = grid[r][c];
    }
  }
}

// ---------------------------------------------------------------
// printGrid()
// Prints 4x4 occupancy grid to Serial.
// [X] = occupied, [ ] = empty
// ---------------------------------------------------------------
void printGrid() {
  Serial.println("--- Defender Board State ---");
  Serial.println("     C0    C1    C2    C3");
  for (int r = 0; r < ROWS; r++) {
    Serial.print("R");
    Serial.print(r);
    Serial.print("  ");
    for (int c = 0; c < COLS; c++) {
      Serial.print(grid[r][c] ? " [X] " : " [ ] ");
    }
    Serial.println();
  }
  Serial.println();
}

// ---------------------------------------------------------------
// printOccupiedCells()
// Lists each occupied cell by coordinate.
// ---------------------------------------------------------------
void printOccupiedCells() {
  bool anyOccupied = false;
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      if (grid[r][c]) {
        Serial.print("  Occupied: row=");
        Serial.print(r);
        Serial.print(" col=");
        Serial.println(c);
        anyOccupied = true;
      }
    }
  }
  if (!anyOccupied) {
    Serial.println("  (no cells occupied)");
  }
  Serial.println();
}

// ---------------------------------------------------------------
// setup()
// ---------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(LOAD_PIN,   OUTPUT);
  pinMode(CLOCK_PIN,  OUTPUT);
  pinMode(CLKINH_PIN, OUTPUT);
  pinMode(DATA_PIN,   INPUT);

  // Idle states
  digitalWrite(LOAD_PIN,   HIGH);   // HIGH between reads
  digitalWrite(CLOCK_PIN,  LOW);
  digitalWrite(CLKINH_PIN, LOW);    // LOW = clock always enabled

  // Clear grids
  memset(grid,     0, sizeof(grid));
  memset(prevGrid, 0, sizeof(prevGrid));

  Serial.println("========================================");
  Serial.println(" Defender Board Test — Single Player");
  Serial.println(" 2x 74HC165 daisy-chained (16 inputs)");
  Serial.println(" Switch ON (HIGH) = Occupied [X]");
  Serial.println("========================================");
  Serial.println();
}

// ---------------------------------------------------------------
// loop()
// Polls at ~5Hz. Only prints when state changes.
// ---------------------------------------------------------------
void loop() {
  uint16_t rawData = readShiftRegisters();
  updateGrid(rawData);

  if (gridChanged()) {
    printGrid();
    printOccupiedCells();
    savePrevGrid();
  }

  delay(200);  // Poll ~5 times per second
}
