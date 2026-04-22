#ifndef BEAMGRID_H
#define BEAMGRID_H

#include <Arduino.h>

// Holds the result of a detected intersection
struct GridHit {
  bool detected;
  int row;  // 1-4
  int col;  // 1-4
};

class BeamGrid {
  public:
    BeamGrid();
    void begin();
    GridHit check();  // call this every loop()

  private:
    static const int ROW_PINS[4];
    static const int COL_PINS[4];
    unsigned long lastRowBreak[4];
    unsigned long lastColBreak[4];
    static const int WINDOW = 1000;
};

#endif
