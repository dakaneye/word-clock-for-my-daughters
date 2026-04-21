#include <Arduino.h>

constexpr int PIN_HOUR = 32;
constexpr int PIN_MIN  = 33;
constexpr int PIN_AUD  = 14;

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(PIN_HOUR, INPUT_PULLUP);
  pinMode(PIN_MIN,  INPUT_PULLUP);
  pinMode(PIN_AUD,  INPUT_PULLUP);
  Serial.println("Button reader -- 1=released, 0=pressed");
}

void loop() {
  int h = digitalRead(PIN_HOUR);
  int m = digitalRead(PIN_MIN);
  int a = digitalRead(PIN_AUD);
  Serial.printf("H=%d  M=%d  A=%d\n", h, m, a);
  delay(100);
}
