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

// Add to your existing handleESP32Serial() or loop():

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
    Serial.printf("Watching for beam break (P%d shot)...\n", player);
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
