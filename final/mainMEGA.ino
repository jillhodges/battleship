// include code for microswitches, beam breaks, LEDs, UART comms with esp32
// pins 2-9: beam breaks
// pins 10-13: LEDs
//pins 22-38: defender board 1
// pins 39-54: defender board 2
// pins 0-1: RX-TX comms with ESP32

// INCLUDE ALL LIBRARIES
#include "BeamGrid.h"    
#include <Adafruit_NeoPixel.h>
#include "ShipPlacement.h"

// ALL OBJECTS:
BeamGrid grid;           // 2. create the object, global scope

#include <Adafruit_NeoPixel.h>

// Attacker strips (show where shots landed from shooter's perspective)
#define P1_ATTACKER_PIN  10
#define P2_ATTACKER_PIN  11
// Defender strips (show hits/misses on defender's board)
#define P1_DEFENDER_PIN  12
#define P2_DEFENDER_PIN  13

// LEDS
#define LEDS_PER_CELL    7
#define TOTAL_LEDS       (16 * LEDS_PER_CELL)  // 112
#define strip_RED   0xFF0000
#define strip_BLUE  0x0000FF
Adafruit_NeoPixel p1AttackStrip(TOTAL_LEDS, P1_ATTACKER_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel p2AttackStrip(TOTAL_LEDS, P2_ATTACKER_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel p1DefendStrip(TOTAL_LEDS, P1_DEFENDER_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel p2DefendStrip(TOTAL_LEDS, P2_DEFENDER_PIN, NEO_GRB + NEO_KHZ800);

//BEAM BREAK
#define BEAM_TIMEOUT_MS  5000  // 5 seconds to detect beam break after shot
int  currentShooter  = 0;
bool waitingForBeam  = false;
unsigned long beamTimeout = 0;



// Convert row/col to LED start index
int cellToLED(int row, int col) {
  return (row * 4 + col) * LEDS_PER_CELL;
}

// Light a cell on a strip
void setStripCell(Adafruit_NeoPixel &strip, int row, int col, uint32_t color) {
  int start = cellToLED(row, col);
  for (int i = 0; i < LEDS_PER_CELL; i++)
    strip.setPixelColor(start + i, color);
  strip.show();
}

// Light result LEDs on both attacker and defender strips
void lightResultLED(int shootingPlayer, bool isHit, int row, int col) {
  uint32_t color = isHit ? strip_RED : strip_BLUE;

  if (shootingPlayer == 1) {
    // P1 shot: light P1 attacker strip + P2 defender strip
    setStripCell(p1AttackStrip, row, col, color);
    setStripCell(p2DefendStrip, row, col, color);
  } else {
    // P2 shot: light P2 attacker strip + P1 defender strip
    setStripCell(p2AttackStrip, row, col, color);
    setStripCell(p1DefendStrip, row, col, color);
  }
}



void clearAllLEDs() {
  p1AttackStrip.clear(); p1AttackStrip.show();
  p2AttackStrip.clear(); p2AttackStrip.show();
  p1DefendStrip.clear(); p1DefendStrip.show();
  p2DefendStrip.clear(); p2DefendStrip.show();
}

void watchBeamBreak() {
  if (!waitingForBeam) return;

  // Check beam break grid (reuse your BeamGrid class)
  GridHit hit = grid.check();

  if (hit.detected) {
    waitingForBeam = false;

    // Was the hit cell occupied by a ship?
    bool isHit = false;
    int defenderPlayer = (currentShooter == 1) ? 2 : 1;
    bool (*defenderBoard)[4] = (defenderPlayer == 1) ? p1Ships : p2Ships;
    isHit = defenderBoard[hit.row - 1][hit.col - 1];

    // Send result to ESP32
    Serial1.print(isHit ? "HIT," : "MISS,");
    Serial1.print(hit.row);
    Serial1.print(",");
    Serial1.println(hit.col);

    Serial.print(isHit ? "HIT" : "MISS");
    Serial.print(" at R");
    Serial.print(hit.row);
    Serial.print("C");
    Serial.println(hit.col);
    return;
  }

  // Timeout - no beam break detected
  if (millis() - beamTimeout > BEAM_TIMEOUT_MS) {
    waitingForBeam = false;
    Serial1.println("MISS,-1,-1");
    Serial.println("Beam timeout - automatic miss.");
  }
}

void handleESP32Serial() {
  if (!Serial1.available()) return;

  String msg = Serial1.readStringUntil('\n');
  msg.trim();
  msg.toUpperCase();

  Serial.print("ESP32: ");
  Serial.println(msg);

  // ESP32 tells Mega a shot was fired by player X
  // Mega now watches beam breaks and reports back
  if (msg.startsWith("FIRE,")) {
    int player = msg.substring(5).toInt();
    currentShooter = player;
    waitingForBeam = true;
    beamTimeout    = millis();
    Serial.print("Watching for beam break (P");
    Serial.print(player);
    Serial.println(" shot)...");
    return;
  }

  // ESP32 tells Mega to light LEDs for result
  // Format: RESULT,<player>,<hit>,<row>,<col>
  if (msg.startsWith("RESULT,")) {
    int player = msg.charAt(7) - '0';
    int isHit  = msg.charAt(9) - '0';
    int row    = msg.substring(11, 12).toInt();
    int col    = msg.substring(13).toInt();

    if (row >= 0 && col >= 0) {
      lightResultLED(player, isHit == 1, row, col);
    }
    return;
  }

  // Game over - turn off all LEDs
  if (msg == "GAME OVER") {
    clearAllLEDs();
    return;
  }

  // Reset - clear everything
  if (msg == "RESET") {
    clearAllLEDs();
    waitingForBeam = false;
    return;
  }
}

void setup() {
  Serial.begin(9600);  // debug output
  Serial1.begin(9600);      // communication with ESP32 (TX1/RX1 pins 18/19)

  grid.begin();          // beam break initialization
  
  // LED initialization
p1AttackStrip.begin();  p1AttackStrip.setBrightness(80);  p1AttackStrip.clear();  p1AttackStrip.show();
p2AttackStrip.begin();  p2AttackStrip.setBrightness(80);  p2AttackStrip.clear();  p2AttackStrip.show();
p1DefendStrip.begin();  p1DefendStrip.setBrightness(80);  p1DefendStrip.clear();  p1DefendStrip.show();
p2DefendStrip.begin();  p2DefendStrip.setBrightness(80);  p2DefendStrip.clear();  p2DefendStrip.show();

  //defender board placement time
  // Microswitch pins - P1 board
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      pinMode(P1_PINS[r][c], INPUT_PULLUP);

  // Microswitch pins - P2 board
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      pinMode(P2_PINS[r][c], INPUT_PULLUP);

  // Ship placement runs here and blocks until both players are done
  runPlacementPhase();


}

void loop() {
  handleESP32Serial();
  watchBeamBreak();
  
}
