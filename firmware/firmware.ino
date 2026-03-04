#include "WiFiManager.h"

WiFiManager wifi("SpectrumSetup-7F", "lightwater958");

void setup() {
  Serial.begin(115200);
  WiFi.begin();
  Serial.println("Hello World ESP32-C3");
}

void loop() {
  delay(1000);
}
