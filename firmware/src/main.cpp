#include <Arduino.h>
#include "wifi_provision.h"

void setup() {
    Serial.begin(115200);
    Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);
    wc::wifi_provision::begin();
}

void loop() {
    wc::wifi_provision::loop();
    delay(1);  // yield to the IDLE task so watchdog + WiFi stacks run
}
