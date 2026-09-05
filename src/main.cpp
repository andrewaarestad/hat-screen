// hat-screen: firmware for a wearable ST7789 strip display.
//
// Hardware: Seeed Studio XIAO ESP32C3 + 2.25" 76x284 ST7789 SPI TFT + 1S LiPo.
// Wiring and power notes live in docs/HARDWARE.md.

#include <Arduino.h>

#include "app/App.h"

namespace {
App app;
bool ready = false;
}  // namespace

void setup() {
  Serial.begin(115200);
#ifdef HAT_DEBUG
  // Give the USB CDC port a moment to enumerate so early logs are not lost.
  const uint32_t deadline = millis() + 2000;
  while (!Serial && millis() < deadline) {
    delay(10);
  }
#endif
  Serial.println("[hat] booting");

  ready = app.begin();
  if (!ready) {
    Serial.println("[hat] init failed; halting");
  }
}

void loop() {
  if (!ready) {
    delay(1000);
    return;
  }
  app.loop();
}
