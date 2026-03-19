// Simple IR break-beam example for Arduino
// 2168 IR break beam sensor (emitter + receiver)

// Change this pin if you wire the signal to another digital input
const int ROW_BEAM_PIN = 6;
const int COL_BEAM_PIN = 7;
unsigned long lastRowBreak = 0;
unsigned long lastColBreak = 0;
const int window = 100; // Time in ms to "remember" a break

void setup() {
  Serial.begin(9600);

  // Configure pin as input with internal pull-up
  pinMode(ROW_BEAM_PIN, INPUT_PULLUP);
  pinMode(COL_BEAM_PIN, INPUT_PULLUP);

  // Optional: built-in LED to indicate beam break
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  bool rowBroken = (digitalRead(ROW_BEAM_PIN) == LOW);
  bool colBroken = (digitalRead(COL_BEAM_PIN) == LOW);
  unsigned long currentTime = millis();

  // Record the timestamp of the most recent break
  if (rowBroken) lastRowBreak = currentTime;
  if (colBroken) lastColBreak = currentTime;

  // Check if BOTH have been broken within the 'window'
  if ((currentTime - lastRowBreak < window) && (currentTime - lastColBreak < window)) {
    Serial.println("INTERSECTION: Ball Detected!");
    
    // Reset timers so we don't spam the "Intersection" message
    lastRowBreak = 0; 
    lastColBreak = 0;
  } 
//  else if (rowBroken) {
//    // Only print if the other beam hasn't been hit recently
//    if (currentTime - lastColBreak > window) {
//      Serial.println("Row break");
//    }
//  } 
//  else if (colBroken) {
//    if (currentTime - lastRowBreak > window) {
//      Serial.println("Column break");
//    }
//  }
}
