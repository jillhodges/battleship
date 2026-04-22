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

void setup() {
  Serial.begin(9600);  // debug output
  Serial1.begin(9600);      // communication with ESP32 (TX1/RX1 pins 18/19)

  grid.begin();          // beam break initialization
  
  // LED initialization
  p1Strip.begin();
  p1Strip.setBrightness(80);
  p1Strip.clear();
  p1Strip.show();

  p2Strip.begin();
  p2Strip.setBrightness(80);
  p2Strip.clear();
  p2Strip.show();

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
  GridHit hit = grid.check();   // 4. call every loop()

  if (hit.detected) {
    Serial.print("INTERSECTION DETECTED: Row ");
    Serial.print(hit.row);
    Serial.print(", Column ");
    Serial.println(hit.col);

    // Send to ESP32 over Serial1
    Serial1.print("HIT,");
    Serial1.print(hit.row);
    Serial1.print(",");
    Serial1.println(hit.col);
  }
}
