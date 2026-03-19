#include <ArduinoShiftIn.h>

// Arguments are: pLoadPin, clockEnablePin (connect this to GND), dataPin, clockPin
// Note: The library example linked in search results shows a different pin order/functionality for the clockEnable pin - refer to the library's specific documentation for pin mapping.
// A common configuration uses 3 pins: Load, Clock, and Data.
// For the standard 74HC165 use: shiftIn(dataPin, clockPin, MSBFIRST) after pulsing the Load pin.

// Example with a typical 3-pin configuration
const int PL_PIN = 4; // Parallel Load pin
const int CLK_PIN = 3; // Clock pin
const int DATA_PIN = 2; // Data In pin

void setup() {
  Serial.begin(9600);
  pinMode(PL_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(DATA_PIN, INPUT);
  digitalWrite(CLK_PIN, LOW);
  digitalWrite(PL_PIN, HIGH);
}

void loop() {
  unsigned int sensorValues = 0;
  // Load the inputs
  digitalWrite(PL_PIN, LOW);
  digitalWrite(PL_PIN, HIGH);
  
  // Read 16 bits
  for (int i = 0; i < 16; i++) {
    // Shift in each bit
    // Note: shiftIn() can be used in a loop, but here is a manual bit read
    digitalWrite(CLK_PIN, HIGH); // Clock high to shift data
    bitWrite(sensorValues, i, digitalRead(DATA_PIN)); // Read the bit
    digitalWrite(CLK_PIN, LOW); // Clock low to prepare for next shift
  }

  Serial.print("Sensor Values: ");
  Serial.println(sensorValues, BIN);
  delay(100);
}
