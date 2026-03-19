/*
 * switch_test.ino
 *
 * Tests a SINGLE micro lever switch directly on one Arduino Mega digital pin.
 *
 * PURPOSE:
 *   Lowest-level sanity check before wiring up shift registers.
 *   Confirms:
 *     1. Switch wiring is correct
 *     2. Switch reads HIGH when pressed / LOW when released (or vice versa)
 *     3. Debounce logic works
 *   If this test passes, your switch hardware is good.
 *   If defender_board_test fails but this passes, the problem is shift register wiring.
 *
 * ---------------------------------------------------------------
 * WIRING — Active HIGH (pull-down, matches defender board wiring)
 * ---------------------------------------------------------------
 *
 *   Switch side A --> Arduino Mega Pin 2
 *   Switch side B --> VCC (5V)
 *   10k resistor  --> Pin 2 to GND (pull-down)
 *
 *   Switch OPEN  (released) = LOW  = empty
 *   Switch CLOSED (pressed) = HIGH = occupied
 *
 * ---------------------------------------------------------------
 * WIRING — Active LOW (pull-up alternative)
 * ---------------------------------------------------------------
 *
 *   Switch side A --> Arduino Mega Pin 2
 *   Switch side B --> GND
 *   Change pinMode to INPUT_PULLUP (uses internal 20k pull-up)
 *
 *   Switch OPEN  (released) = HIGH = empty
 *   Switch CLOSED (pressed) = LOW  = occupied
 *
 *   If using this wiring, flip SWITCH_ACTIVE_STATE to LOW below.
 *
 * ---------------------------------------------------------------
 * NOTE:
 *   Your defender board uses active HIGH (pull-down resistors).
 *   Keep this test consistent with that wiring.
 * ---------------------------------------------------------------
 */

// ---------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------
#define SWITCH_PIN          2      // Digital pin connected to switch
#define SWITCH_ACTIVE_STATE HIGH   // HIGH = occupied (active HIGH / pull-down)
                                   // Change to LOW if using INPUT_PULLUP wiring

#define DEBOUNCE_DELAY_MS   20     // Milliseconds to debounce

// ---------------------------------------------------------------
// State tracking
// ---------------------------------------------------------------
bool switchState     = false;  // Current debounced state
bool lastRawState    = false;  // Last raw read
bool prevSwitchState = false;  // Previous debounced state (for change detection)
unsigned long lastDebounceTime = 0;

// ---------------------------------------------------------------
// readSwitch()
// Returns debounced switch state.
// true  = occupied (switch active)
// false = empty    (switch inactive)
// ---------------------------------------------------------------
bool readSwitch() {
  bool rawState = (digitalRead(SWITCH_PIN) == SWITCH_ACTIVE_STATE);

  // If raw reading changed, reset debounce timer
  if (rawState != lastRawState) {
    lastDebounceTime = millis();
    lastRawState = rawState;
  }

  // Only accept new state if it has been stable long enough
  if ((millis() - lastDebounceTime) >= DEBOUNCE_DELAY_MS) {
    switchState = rawState;
  }

  return switchState;
}

// ---------------------------------------------------------------
// setup()
// ---------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(SWITCH_PIN, INPUT);  // Use INPUT_PULLUP if using active LOW wiring

  Serial.println("========================================");
  Serial.println(" Single Micro Lever Switch Test");
  Serial.println(" Pin: " + String(SWITCH_PIN));
  Serial.println(" Active state: " + String(SWITCH_ACTIVE_STATE == HIGH ? "HIGH (pull-down)" : "LOW (pull-up)"));
  Serial.println(" Debounce: " + String(DEBOUNCE_DELAY_MS) + "ms");
  Serial.println("========================================");
  Serial.println(" Press and release the switch.");
  Serial.println(" You should see state changes printed below.");
  Serial.println();
}

// ---------------------------------------------------------------
// loop()
// Only prints when state changes, so Serial stays readable.
// ---------------------------------------------------------------
void loop() {
  bool current = readSwitch();

  if (current != prevSwitchState) {
    if (current) {
      Serial.println(">> OCCUPIED  (switch closed - ship placed)");
    } else {
      Serial.println(">> EMPTY     (switch open - no ship)");
    }
    prevSwitchState = current;
  }
}
