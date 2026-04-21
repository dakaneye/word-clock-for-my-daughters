#include <Arduino.h>
#include <Wire.h>

void scan() {
  Serial.println("I2C scan:");
  int count = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  found 0x%02X\n", addr);
      count++;
    }
  }
  Serial.printf("done -- %d device(s)\n", count);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(21, 22);  // SDA=GPIO21, SCL=GPIO22
  delay(100);
}

void loop() {
  scan();
  delay(2000);
}
