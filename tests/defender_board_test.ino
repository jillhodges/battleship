/*
 * 4x4 Switch Grid via 74HC165 Shift Registers
 * Arduino Mega
 *
 * WIRING (74HC165 chips, daisy-chained):
 *   Use 2x 74HC165 chips (16 inputs total, all 16 used).
 *
 *   Pin connections (both chips share these control lines):
 *     Arduino Pin 8  --> SH/!LD (Pin 1 on both chips, tied together)
 *     Arduino Pin 9  --> CLK    (Pin 2 on both chips, tied together)
 *     Arduino Pin 10 --> CLK INH (Pin 15 on both chips, tie to GND for always-enabled,
 *                                 or tie to Arduino pin 10 for control)
 *
 *   Data chain (daisy-chained serial output):
 *     Chip 1 QH (Pin 9)  --> Chip 2 SER (Pin 10)
 *     Chip 2 QH (Pin 9)  --> Arduino Pin 11 (MISO / data in)
 *
 *   Chip 1 SER (Pin 10) --> GND
 *
 *   Switch wiring per input pin on each 74HC165:
 *     Switch one side --> Chip parallel input pin (A-H, pins 11-14 and 3-6)
 *     Switch other side --> VCC (5V)
 *     10k pull-down resistor from input pin to GND
 *     (Switch ON = HIGH = occupied)
 *
 *   Chip 1 inputs  = Grid positions [0][0] through [1][3]  (first 8 cells)
 *   Chip 2 inputs  = Grid positions [2][0] through [3][3]  (last 8 cells)
 *
 * SWITCH-TO-BIT MAPPING:
 *   Bit 15 (first clocked out from Chip 1) = grid[0][0]
 *   Bit 14 = grid[0][1] ... through to Bit 0 = grid[3][3]
 */

// ----- Pin definitions -----
#define LOAD_PIN   8    // SH/!LD: LOW pulses to latch parallel inputs
#define CLOCK_PIN  9    // Clock for shifting data out
#define DATA_PIN   11   // Serial data from last chip's QH

// ----- Grid dimensions -----
#define ROWS 4
#define COLS 4
#define NUM_CHIPS 2     // 2x 74HC165 = 16 bits; all 16 used for 4x4 grid

// ----- Grid state -----
bool grid[ROWS][COLS];

// ----- Function: read all shift registers into a 32-bit value -----
uint32_t readShiftRegisters() {
  uint32_t data = 0;

  // Pulse LOAD low to latch all parallel inputs simultaneously
  digitalWrite(LOAD_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(LOAD_PIN, HIGH);

  // Shift in 16 bits (2 chips x 8 bits), MSB first
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

// ----- Function: map 16-bit shift register data to 4x4 grid -----
void updateGrid(uint32_t rawData) {
  /*
   * Bit 15 is clocked out first (Chip 1, input A).
   * We map bits 15 down to 0 (16 bits) to grid[row][col].
   * Bit position for grid[r][c] = 15 - (r * COLS + c)
   *
   * Switch is ACTIVE HIGH (pulled to VCC when closed).
   * Adjust to `== LOW` if using pull-ups / active-low switches.
   */
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      int bitIndex = 15 - (r * COLS + c);
      grid[r][c] = (rawData >> bitIndex) & 0x01;  // 1 = switch ON = occupied
    }
  }
}

// ----- Function: print grid to Serial (for debugging) -----
void printGrid() {
  Serial.println("--- Grid State ---");
  Serial.println("     C0   C1   C2   C3");
  for (int r = 0; r < ROWS; r++) {
    Serial.print("R");
    Serial.print(r);
    Serial.print(" ");
    for (int c = 0; c < COLS; c++) {
      Serial.print("  ");
      Serial.print(grid[r][c] ? "[X]" : "[ ]");
    }
    Serial.println();
  }
  Serial.println();
}

// ----- Setup -----
void setup() {
  Serial.begin(115200);

  pinMode(LOAD_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, INPUT);

  // Initial pin states
  digitalWrite(LOAD_PIN, HIGH);   // Keep HIGH between reads (idle)
  digitalWrite(CLOCK_PIN, LOW);

  Serial.println("4x4 Switch Grid Ready.");
  Serial.println("Wiring: 2x 74HC165 daisy-chained.");
  Serial.println("Switch ON (HIGH) = Occupied [X]");
  Serial.println();
}

// ----- Main loop -----
void loop() {
  uint32_t rawData = readShiftRegisters();
  updateGrid(rawData);
  printGrid();

  // -------------------------------------------------------
  // Example: react to a specific cell being occupied
  // -------------------------------------------------------
  if (grid[1][1]) {
    // Center cell is occupied — do something
    // e.g. digitalWrite(LED_PIN, HIGH);
  }

  // -------------------------------------------------------
  // Example: scan all occupied cells
  // -------------------------------------------------------
  /*
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      if (grid[r][c]) {
        Serial.print("Occupied: row=");
        Serial.print(r);
        Serial.print(" col=");
        Serial.println(c);
      }
    }
  }
  */

  delay(200);   // Poll ~5 times per second; adjust as needed
}
