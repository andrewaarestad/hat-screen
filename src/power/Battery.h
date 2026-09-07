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

  // Forces an immediate sample, ignoring the rate limit. For the boot-time
  // cutoff check, which cannot wait for the normal cadence.
  void sampleNow();

  bool valid() const { return _valid; }

  // Volts at the cell, after undoing the divider.
  float volts() const { return _volts; }

  // 0-100, from a discharge curve rather than a linear 3.3-4.2 V map, which
  // would spend most of its range in a region the cell passes through quickly.
  uint8_t percent() const;

  // True below VBAT_LOW_VOLTS: time to dim the backlight and warn.
  bool low() const;

  // True when the reading is trustworthy enough to power the board down over.
  // A reading below VBAT_PLAUSIBLE_MIN_VOLTS means no cell or no divider, not
  // a critically flat battery -- see board_config.h.
  bool cutoffArmed() const;

 private:
  float readVolts() const;

  float _volts = 0.0f;
  bool _valid = false;
  uint32_t _lastSampleMs = 0;
};
