#include <Adafruit_NeoPixel.h>

#define PIN        6  // Data pin
#define NUMPIXELS 60  // Total number of LEDs

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  pixels.begin();
  pixels.setBrightness(150); 
}

void loop() {
  for(int i = 0; i < NUMPIXELS; i++) {
    // Check the position within a group of 10
    // If the remainder of (i / 10) is less than 5, it's a Red LED
    if ((i % 10) < 5) {
      pixels.setPixelColor(i, pixels.Color(255, 0, 0)); // Red
    } 
    // Otherwise, it's a Blue LED
    else {
      pixels.setPixelColor(i, pixels.Color(0, 0, 255)); // Blue
    }
  }

  pixels.show(); // Push the data to the strip
  delay(50);     // Small delay to save processing power
}
