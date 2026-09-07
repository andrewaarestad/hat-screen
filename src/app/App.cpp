#include "app/App.h"

#include <Arduino.h>

#include "board_config.h"

namespace {
constexpr uint32_t kDebounceMs = 30;
}  // namespace

bool App::begin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  _battery.begin();

  // Before the panel is lit: if this boot is a wake from a low-voltage sleep
  // and the cell has not recovered, this goes straight back to sleep.
  PowerManager::guardBoot(_battery);

  if (!_display.begin()) {
    Serial.println("[hat] display init failed (canvas allocation?)");
    return false;
  }
  _power.begin(_display, _battery);

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
  // Once the low-voltage notice is up, the only thing left to do is sleep.
  if (_power.state() == PowerManager::State::Shutdown) return;

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

  // Returns false once the low-voltage shutdown owns the display.
  if (!_power.update(dtMs)) return;

  if (SCENE_AUTO_CYCLE_MS > 0) {
    _sceneElapsedMs += dtMs;
    if (_sceneElapsedMs >= SCENE_AUTO_CYCLE_MS) {
      nextScene();
    }
  }

  current().update(_display, dtMs);
}
