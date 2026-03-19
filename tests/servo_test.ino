#include <ESP32Servo.h>

Servo servo1;

void setup() {
  Serial.begin(115200);
  Serial.println("Servo test starting...");
  
  servo1.attach(18, 900, 2100);
  Serial.println("Servo attached.");
}

void loop() {
  Serial.println("Moving to 0 degrees");
  servo1.writeMicroseconds(900);
  delay(2000);
  
  Serial.println("Moving to 90 degrees");
  servo1.writeMicroseconds(1500);
  delay(2000);
  
  Serial.println("Moving to 180 degrees");
  servo1.writeMicroseconds(2100);
  delay(2000);
}
