#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

constexpr int PIN_SD_CS = 5;

void listRoot() {
  Serial.println("---");
  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("SD init FAILED");
    return;
  }
  Serial.println("SD init ok");

  File root = SD.open("/");
  if (!root) {
    Serial.println("cannot open root");
    return;
  }

  int count = 0;
  File f = root.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      Serial.printf("  %s  %lu bytes\n", f.name(), (unsigned long)f.size());
      count++;
    }
    f = root.openNextFile();
  }
  root.close();
  Serial.printf("done -- %d file(s)\n", count);

  SD.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);
}

void loop() {
  listRoot();
  delay(3000);
}
