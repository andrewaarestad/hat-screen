#include "power/Battery.h"

#include <Arduino.h>

#include "board_config.h"

namespace {

constexpr uint32_t kSampleIntervalMs = 2000;
constexpr uint8_t kOversample = 8;
constexpr float kLowVoltage = 3.40f;

// Exponential smoothing keeps the reading from jumping when the backlight PWM
// or a WiFi transmit burst sags the rail.
constexpr float kSmoothing = 0.20f;

// Resting-voltage discharge curve for a generic 1S LiPo, 0% .. 100% in 10%
// steps. Under load the cell sags, so treat this as indicative, not a gauge.
constexpr float kCurve[11] = {3.27f, 3.61f, 3.69f, 3.71f, 3.73f, 3.75f,
                              3.77f, 3.79f, 3.83f, 3.87f, 4.20f};

}  // namespace

void Battery::begin() {
  if (!VBAT_SENSE_FITTED) return;
  pinMode(PIN_VBAT_SENSE, INPUT);
  // Widest input range: the divided cell voltage tops out around 2.1 V.
  analogSetPinAttenuation(PIN_VBAT_SENSE, ADC_11db);
  _volts = readVolts();
  _valid = true;
  _lastSampleMs = millis();
}

float Battery::readVolts() const {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < kOversample; ++i) {
    sum += analogReadMilliVolts(PIN_VBAT_SENSE);
  }
  const float pinVolts = (sum / static_cast<float>(kOversample)) / 1000.0f;
  return pinVolts * VBAT_DIVIDER_RATIO;
}

void Battery::update() {
  if (!VBAT_SENSE_FITTED) return;

  const uint32_t now = millis();
  if (_valid && (now - _lastSampleMs) < kSampleIntervalMs) return;
  _lastSampleMs = now;

  const float sample = readVolts();
  _volts = _valid ? (_volts + kSmoothing * (sample - _volts)) : sample;
  _valid = true;
}

uint8_t Battery::percent() const {
  if (!_valid) return 0;
  if (_volts <= kCurve[0]) return 0;
  if (_volts >= kCurve[10]) return 100;

  for (uint8_t i = 1; i <= 10; ++i) {
    if (_volts < kCurve[i]) {
      const float span = kCurve[i] - kCurve[i - 1];
      const float within = (_volts - kCurve[i - 1]) / span;
      return static_cast<uint8_t>((i - 1) * 10 + within * 10.0f);
    }
  }
  return 100;
}

bool Battery::low() const { return _valid && _volts < kLowVoltage; }
