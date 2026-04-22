// include code for microswitches, beam breaks, LEDs, UART comms with esp32
// pins 2-9: beam breaks
// pins 10-13: LEDs
//pins 22-38: defender board 1
// pins 39-54: defender board 2
// pins 0-1: RX-TX comms with ESP32

#include "BeamGrid.h"    // 1. include at the top

BeamGrid grid;           // 2. create the object, global scope

void setup() {
  Serial.begin(9600);
  grid.begin();          // 3. call in setup()
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
