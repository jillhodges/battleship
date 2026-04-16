// IR break-beam grid detection for Arduino Mega
// 4 row sensors + 4 column sensors = 4x4 grid
// Detects which row/column intersection was triggered

const int ROW_PINS[4] = {2, 3, 4, 5};
const int COL_PINS[4] = {6, 7, 8, 9};

unsigned long lastRowBreak[4] = {0, 0, 0, 0};
unsigned long lastColBreak[4] = {0, 0, 0, 0};

const int WINDOW = 100; // ms to "remember" a break

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < 4; i++) {
    pinMode(ROW_PINS[i], INPUT_PULLUP);
    pinMode(COL_PINS[i], INPUT_PULLUP);
  }
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println("4x4 beam grid ready.");
}

void loop() {
  unsigned long currentTime = millis();

  // Update break timestamps for all rows and columns
  for (int i = 0; i < 4; i++) {
    if (digitalRead(ROW_PINS[i]) == LOW) lastRowBreak[i] = currentTime;
    if (digitalRead(COL_PINS[i]) == LOW) lastColBreak[i] = currentTime;
  }

  // Check every row/column combination for a coincident break
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if ((currentTime - lastRowBreak[r] < WINDOW) &&
          (currentTime - lastColBreak[c] < WINDOW)) {

        Serial.print("INTERSECTION DETECTED: Row ");
        Serial.print(r + 1);
        Serial.print(", Column ");
        Serial.println(c + 1);

        // Reset both so we don't spam the same hit
        lastRowBreak[r] = 0;
        lastColBreak[c] = 0;
      }
    }
  }
}
