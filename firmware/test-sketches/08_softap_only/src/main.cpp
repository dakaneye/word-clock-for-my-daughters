// Probe: link the WiFi library but never call any WiFi function.
// If this garbles serial, the problem is library init / link-time.
// If serial stays clean, the problem is specifically WiFi runtime calls.
#include <Arduino.h>
#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("== link-WiFi-but-never-call: alive ==");
}

void loop() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last >= 1000) {
    last = now;
    Serial.printf("tick t=%lus\n", now / 1000);
  }
  delay(1);
}
