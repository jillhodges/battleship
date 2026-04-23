#include <Bluepad32.h>
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "CommProtocol.h"
#include <Adafruit_NeoPixel.h>

///////////////////////////////////////////////////////////
/////////// LED DEFINTIONS //////////////
///////////////////////////////////////////////////////////

// LED pin defintions
#define P1_ATK_PIN 10
#define P1_DEF_PIN 11
#define P2_ATK_PIN 12
#define P2_DEF_PIN 13

// total number of LEDs per strip
#define TOTAL_LEDS 112 // 16 cells * 7 LEDs

// Define the Strips
Adafruit_NeoPixel P1_ATK_STRIP(TOTAL_LEDS, P1_ATK_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel P1_DEF_STRIP(TOTAL_LEDS, P1_DEF_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel P2_ATK_STRIP(TOTAL_LEDS, P2_ATK_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel P2_DEF_STRIP(TOTAL_LEDS, P2_DEF_PIN, NEO_GRB + NEO_KHZ800);

///////////////////////////////////////////////////////////
/////////////// END LED DEFINIONS /////////////////////////
///////////////////////////////////////////////////////////



////////////////////////////////////////////
////FUNCTION FOR RECEIVING DATA FROM ESP////
////////////////////////////////////////////
void receiveFromESP() {
  // We need at least 10 bytes for a full packet
  if (Serial1.available() < 10) return;

  // Look for the start byte
  if (Serial1.peek() != START_BYTE) {
    Serial1.read(); // Discard junk byte
    return;
  }

  uint8_t buf[10];
  Serial1.readBytes(buf, 10);

  // Validate Start/End bytes
  if (buf[0] != START_BYTE || buf[9] != END_BYTE) return;

  // Verify Checksum (connected ^ lx1 ^ ly1 ^ lx2 ^ ly2 ^ btn1 ^ btn2)
  uint8_t calcChecksum = buf[1] ^ buf[2] ^ buf[3] ^ buf[4] ^ buf[5] ^ buf[6] ^ buf[7];
  if (calcChecksum != buf[8]) {
    Serial.println("Mega Error: Bad Checksum from ESP32");
    return;
  }

  // Success! Extract the button states
  p1_buttons = buf[6];
  p2_buttons = buf[7];

  // Logic: Detect if the "Cross" button (Bit 0) was JUST pressed
  // This prevents one click from firing 100 times
  if ((p1_buttons & 0x01) && !(last_p1_buttons & 0x01)) {
    Serial.println("P1 Pressed FIRE!");
    // Here you would call your game logic, e.g.:
    // resolveShot(1, currentRow, currentCol);
  }

  last_p1_buttons = p1_buttons;
  last_p2_buttons = p2_buttons;
}

////////////////////////////////////////////
////END FUNCTION FOR RECEIVING DATA FROM ESP////
////////////////////////////////////////////


// 2. The Structures
struct Ship {
  int size;
  int hitsReceived;
  bool isSunk;
  int coordinates[2]; 

  void reset(int s) {
    size = s;
    hitsReceived = 0;
    isSunk = false;
  }
};

class Player {
  private:
    Adafruit_NeoPixel* attackerStrip;
    Adafruit_NeoPixel* defenderStrip;
    int totalShots = 0;
    int shipsSunk = 0;
    bool allShipsSunk = false;
    int ledStatus[16]; 
    Ship ships[3]; 

  public:
    int playerNum;

    Player(int num, Adafruit_NeoPixel& atk, Adafruit_NeoPixel& def) {
      playerNum = num;
      attackerStrip = &atk;
      defenderStrip = &def;
      ships[0].reset(2);
      ships[1].reset(2);
      ships[2].reset(1);
      for(int i=0; i<16; i++) ledStatus[i] = 0;
    }

    void recordShot() { totalShots++; }

    bool checkIncomingShot(int row, int col) {
      int shotCoord = (row * 4) + col;
      for (int i = 0; i < 3; i++) {
        for (int j = 0; j < ships[i].size; j++) {
          if (ships[i].coordinates[j] == shotCoord) {
            ships[i].hitsReceived++;
            checkShipStatus(i);
            return true;
          }
        }
      }
      return false;
    }

    void checkShipStatus(int shipIdx) {
      if (ships[shipIdx].hitsReceived >= ships[shipIdx].size && !ships[shipIdx].isSunk) {
        ships[shipIdx].isSunk = true;
        shipsSunk++;
        if (shipsSunk >= 3) allShipsSunk = true;
      }
    }

    bool hasLost() { return allShipsSunk; }
    
    void updateLEDs(int row, int col, uint32_t color, bool isAttackerBoard) {
      Adafruit_NeoPixel* targetStrip = isAttackerBoard ? attackerStrip : defenderStrip;
      int startLED = (row * 4 + col) * 7;
      for (int i = 0; i < 7; i++) {
        targetStrip->setPixelColor(startLED + i, color);
      }
      targetStrip->show();
    }
};

// 3. Create the Players (MUST be here, below class but above functions)
Player p1(1, P1_ATK_STRIP, P1_DEF_STRIP);
Player p2(2, P2_ATK_STRIP, P2_DEF_STRIP);

// 4. Resolve Shot Function
void resolveShot(int shooter, int row, int col) {
    uint32_t c_HIT_GREEN = 0x00FF00;
    uint32_t c_HIT_RED   = 0xFF0000;
    uint32_t c_MISS_BLUE  = 0x0000FF;

    Player& attacker = (shooter == 1) ? p1 : p2;
    Player& defender = (shooter == 1) ? p2 : p1;

    attacker.recordShot();
    bool hit = defender.checkIncomingShot(row, col);

    if (hit) {
        attacker.updateLEDs(row, col, c_HIT_GREEN, true); 
        defender.updateLEDs(row, col, c_HIT_RED, false); 
    } else {
        attacker.updateLEDs(row, col, c_MISS_BLUE, true);
        defender.updateLEDs(row, col, c_MISS_BLUE, false);
    }
}



/////////////////////////////////////////////////////

///////////END FUNCTION DEFINTIONS///////////////////

/////////////////////////////////////////////////////




void setup() {
  Serial.begin(9600); // to send outputs to the computer
  Serial1.begin(115200); // to connect to the ESP32

  // --- CRITICAL: Initialize the strips first! ---
  P1_ATK_STRIP.begin();
  P1_DEF_STRIP.begin();
  P2_ATK_STRIP.begin();
  P2_DEF_STRIP.begin();

  // Set brightness so we don't melt anything (0-255)
  P1_ATK_STRIP.setBrightness(150); 
  P1_DEF_STRIP.setBrightness(150);
  P2_ATK_STRIP.setBrightness(150);
  P2_DEF_STRIP.setBrightness(150);

  // 4. THE GRID SQUARE TEST
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      
      uint32_t testColor;
      int cellIndex = (row * 4) + col;

      // Logic: Red, Blue, Gray pattern
      if (cellIndex % 3 == 0) {
        testColor = P1_ATK_STRIP.Color(255, 0, 0);   // Red
      } else if (cellIndex % 3 == 1) {
        testColor = P1_ATK_STRIP.Color(0, 0, 255);   // Blue
      } else {
        testColor = P1_ATK_STRIP.Color(0, 255, 0);  // Gray
      }

      int startLED = cellIndex * 7;
      for (int i = 0; i < 7; i++) {
        P1_ATK_STRIP.setPixelColor(startLED + i, testColor);
        P1_DEF_STRIP.setPixelColor(startLED + i, testColor);
        P2_ATK_STRIP.setPixelColor(startLED + i, testColor);
        P2_DEF_STRIP.setPixelColor(startLED + i, testColor);
      }
    }
  }

  // 5. Push data
  P1_ATK_STRIP.show();
  P1_DEF_STRIP.show();
  P2_ATK_STRIP.show();
  P2_DEF_STRIP.show();
  
  delay(5000); 
  Serial.println("Grid Test Complete.");
}

void loop() {
  // We leave this empty for now so the code can compile.
  // In the future, this is where we will listen for the ESP32.


  receiveFromESP(); // this function handles all the data from ESP32. It also calls other functions for game controller Logic

}