#include "BeamGrid.h"

const int BeamGrid::ROW_PINS[4] = {9, 8, 7, 6};
const int BeamGrid::COL_PINS[4] = {2, 3, 4, 5};

BeamGrid::BeamGrid() {
  for (int i = 0; i < 4; i++) {
    lastRowBreak[i] = 0;
    lastColBreak[i] = 0;
  }
}

void BeamGrid::begin() {
  for (int i = 0; i < 4; i++) {
    pinMode(ROW_PINS[i], INPUT_PULLUP);
    pinMode(COL_PINS[i], INPUT_PULLUP);
  }
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println("4x4 beam grid ready.");
}

GridHit BeamGrid::check() {
  GridHit result = {false, 0, 0};
  unsigned long currentTime = millis();

  for (int i = 0; i < 4; i++) {
    if (digitalRead(ROW_PINS[i]) == LOW) lastRowBreak[i] = currentTime;
    if (digitalRead(COL_PINS[i]) == LOW) lastColBreak[i] = currentTime;
  }

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if ((currentTime - lastRowBreak[r] < WINDOW) &&
          (currentTime - lastColBreak[c] < WINDOW)) {
        result.detected = true;
        result.row = r + 1;
        result.col = c + 1;
        lastRowBreak[r] = 0;
        lastColBreak[c] = 0;
        return result; // return immediately on first hit found
      }
    }
  }

  return result;
}
