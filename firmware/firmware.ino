#include "WiFiManager.h"

WiFiManager wifi("SpectrumSetup-7F", "lightwater958");

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  WiFi.begin();
  Serial.println("Hello World ESP32-C3");
}

void loop() {
  delay(1000);
  Serial.println("Hello World ESP32-C3");
}
