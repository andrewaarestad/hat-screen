#pragma once

#include <stdint.h>

// Single-cell LiPo monitor for the pack wired to the XIAO's BAT pads.
//
// The XIAO ESP32C3 does not expose battery voltage on-board, so this reads an
// external divider (see PIN_VBAT_SENSE in board_config.h). With no divider
// fitted, set VBAT_SENSE_FITTED to false and valid() stays false.
class Battery {
 public:
  void begin();

  // Cheap; safe to call every frame. Samples at most once every 2 s.
  void update();

  bool valid() const { return _valid; }

  // Volts at the cell, after undoing the divider.
  float volts() const { return _volts; }

  // 0-100, from a discharge curve rather than a linear 3.3-4.2 V map, which
  // would spend most of its range in a region the cell passes through quickly.
  uint8_t percent() const;

  // True below roughly 3.4 V: time to dim the backlight and warn.
  bool low() const;

 private:
  float readVolts() const;

  float _volts = 0.0f;
  bool _valid = false;
  uint32_t _lastSampleMs = 0;
};
