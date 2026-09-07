#include "power/PowerManager.h"

#include <Arduino.h>
#include <esp_sleep.h>

#include "board_config.h"
#include "display/Display.h"
#include "power/Battery.h"

namespace {

constexpr uint8_t kLowBacklight = 40;

// Timer wake is the only source armed, so any timer wake means "this is the
// low-voltage recheck". Note that the scene-switch button cannot be used here:
// only GPIO0-GPIO5 are RTC-capable on the ESP32-C3 and the button is on GPIO20.
// See docs/HARDWARE.md if you want a wake button.
void sleepUntilRecheck() {
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(VBAT_SLEEP_RECHECK_S) *
                                1000000ULL);
  esp_deep_sleep_start();
}

}  // namespace

void PowerManager::guardBoot(Battery& battery) {
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) return;

  // Let the rail settle after the reset before trusting the ADC.
  delay(50);
  battery.sampleNow();

  // A reading we cannot trust is not grounds for sleeping through a boot; fall
  // through and let the board come up so it can at least be flashed.
  if (!battery.cutoffArmed()) return;
  if (battery.volts() >= VBAT_RESUME_VOLTS) {
    Serial.printf("[power] recovered at %.2fV, resuming\n",
                  static_cast<double>(battery.volts()));
    return;
  }

  Serial.printf("[power] still low at %.2fV, back to sleep\n",
                static_cast<double>(battery.volts()));
  Serial.flush();
  sleepUntilRecheck();
}

void PowerManager::begin(Display& display, Battery& battery) {
  _display = &display;
  _battery = &battery;
  _state = State::Normal;
  _display->setBacklight(BACKLIGHT_DEFAULT);
}

bool PowerManager::update(uint32_t dtMs) {
  if (_state == State::Shutdown) {
    _shutdownElapsedMs += dtMs;
    if (!_noticeDrawn) {
      drawShutdownNotice();
      _noticeDrawn = true;
    }
    if (_shutdownElapsedMs >= VBAT_SHUTDOWN_NOTICE_MS) {
      powerDown();  // does not return
    }
    return false;
  }

  // Require the cell to stay below the cutoff continuously. A single sagging
  // sample during a radio burst is not a flat battery.
  if (_battery->cutoffArmed() && _battery->volts() < VBAT_CUTOFF_VOLTS) {
    _belowCutoffMs += dtMs;
    if (_belowCutoffMs >= VBAT_CUTOFF_CONFIRM_MS) {
      enterShutdown();
      return false;
    }
  } else {
    _belowCutoffMs = 0;
  }

  const State wanted = _battery->low() ? State::Low : State::Normal;
  if (wanted != _state) {
    _state = wanted;
    _display->setBacklight(_state == State::Low ? kLowBacklight
                                                : BACKLIGHT_DEFAULT);
  }
  return true;
}

void PowerManager::enterShutdown() {
  _state = State::Shutdown;
  _shutdownElapsedMs = 0;
  _noticeDrawn = false;
  Serial.printf("[power] cutoff at %.2fV, shutting down\n",
                static_cast<double>(_battery->volts()));
}

void PowerManager::drawShutdownNotice() {
  // Bright enough to be seen and short-lived enough not to matter.
  _display->setBacklight(BACKLIGHT_DEFAULT);

  TFT_eSprite& canvas = _display->canvas();
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);

  const int16_t midX = _display->width() / 2;
  canvas.setTextColor(TFT_RED, TFT_BLACK);
  canvas.drawString("LOW BATTERY", midX, _display->height() / 2 - 16, 4);

  char buf[32];
  snprintf(buf, sizeof(buf), "%.2fV - sleeping",
           static_cast<double>(_battery->volts()));
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.drawString(buf, midX, _display->height() / 2 + 14, 2);

  _display->present();
}

void PowerManager::powerDown() {
  _display->powerOff();
  Serial.println("[power] deep sleep");
  Serial.flush();
  sleepUntilRecheck();
}
