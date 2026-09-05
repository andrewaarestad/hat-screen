#pragma once

#include "app/Scene.h"
#include "power/Battery.h"

// Battery gauge and uptime. Mostly here as a worked example of laying out a
// static scene on a very wide, very short display.
class StatusScene : public Scene {
 public:
  explicit StatusScene(const Battery& battery) : _battery(battery) {}

  const char* name() const override { return "status"; }

  void onEnter(Display& display) override;
  void update(Display& display, uint32_t dtMs) override;

 private:
  void drawGauge(TFT_eSprite& canvas, int16_t x, int16_t y, int16_t w,
                 int16_t h, uint8_t percent) const;

  const Battery& _battery;
  uint32_t _sinceRedrawMs = 0;
  bool _dirty = true;
};
