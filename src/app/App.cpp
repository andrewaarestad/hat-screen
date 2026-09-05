#include "app/App.h"

#include <Arduino.h>

#include "board_config.h"

namespace {
constexpr uint32_t kDebounceMs = 30;
constexpr uint8_t kLowBatteryBacklight = 40;
}  // namespace

bool App::begin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  if (!_display.begin()) {
    Serial.println("[hat] display init failed (canvas allocation?)");
    return false;
  }
  _battery.begin();

#ifdef HAT_CALIBRATE
  // Held here forever on purpose: adjust PANEL_COL_OFFSET / PANEL_ROW_OFFSET in
  // board_config.h until the white frame lands on the physical panel edge.
  _display.drawCalibrationFrame();
  for (;;) {
    delay(1000);
  }
#endif

  _lastFrameMs = millis();
  current().onEnter(_display);
  return true;
}

void App::pollButton() {
  const bool down = digitalRead(PIN_BUTTON) == LOW;
  const uint32_t now = millis();

  if (down == _buttonWasDown) return;
  if (now - _buttonChangedMs < kDebounceMs) return;

  _buttonChangedMs = now;
  _buttonWasDown = down;

  // Act on press, not release, so the hat feels responsive.
  if (down && !current().onButtonPress()) {
    nextScene();
  }
}

void App::nextScene() {
  current().onExit(_display);
  _sceneIndex = (_sceneIndex + 1) % (sizeof(_scenes) / sizeof(_scenes[0]));
  _sceneElapsedMs = 0;
  current().onEnter(_display);
#ifdef HAT_DEBUG
  Serial.printf("[hat] scene -> %s\n", current().name());
#endif
}

// Dim hard when the cell gets low: the backlight is the biggest single draw, so
// this buys meaningful runtime and doubles as a visible warning.
void App::applyPowerPolicy() {
  const bool low = _battery.low();
  if (low == _dimmedForLowBattery) return;
  _dimmedForLowBattery = low;
  _display.setBacklight(low ? kLowBatteryBacklight : BACKLIGHT_DEFAULT);
}

void App::loop() {
  const uint32_t now = millis();
  const uint32_t dtMs = now - _lastFrameMs;
  if (dtMs < FRAME_INTERVAL_MS) {
    pollButton();
    // Idle the core rather than spinning; keeps average current down.
    delay(1);
    return;
  }
  _lastFrameMs = now;

  pollButton();
  _battery.update();
  applyPowerPolicy();

  if (SCENE_AUTO_CYCLE_MS > 0) {
    _sceneElapsedMs += dtMs;
    if (_sceneElapsedMs >= SCENE_AUTO_CYCLE_MS) {
      nextScene();
    }
  }

  current().update(_display, dtMs);
}
