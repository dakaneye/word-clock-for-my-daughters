#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 alive");
  pinMode(2, OUTPUT);
}

void loop() {
  digitalWrite(2, HIGH);
  delay(500);
  digitalWrite(2, LOW);
  delay(500);
  Serial.println("blink");
}
