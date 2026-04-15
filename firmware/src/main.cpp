#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);
}

void loop() {
  delay(1000);
}
