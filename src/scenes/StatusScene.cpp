#include "scenes/StatusScene.h"

namespace {

constexpr uint32_t kRedrawIntervalMs = 500;
constexpr int16_t kMargin = 6;

uint16_t gaugeColor(uint8_t percent) {
  if (percent < 15) return TFT_RED;
  if (percent < 40) return TFT_ORANGE;
  return TFT_GREEN;
}

}  // namespace

void StatusScene::onEnter(Display& display) {
  (void)display;
  _sinceRedrawMs = kRedrawIntervalMs;
  _dirty = true;
}

void StatusScene::drawGauge(TFT_eSprite& canvas, int16_t x, int16_t y,
                            int16_t w, int16_t h, uint8_t percent) const {
  canvas.drawRect(x, y, w, h, TFT_WHITE);
  // Battery nub on the right-hand end.
  canvas.fillRect(x + w, y + h / 4, 3, h / 2, TFT_WHITE);

  const int16_t fillW = static_cast<int16_t>((w - 4) * (percent / 100.0f));
  if (fillW > 0) {
    canvas.fillRect(x + 2, y + 2, fillW, h - 4, gaugeColor(percent));
  }
}

void StatusScene::update(Display& display, uint32_t dtMs) {
  // Nothing here animates, so redraw twice a second instead of every frame.
  _sinceRedrawMs += dtMs;
  if (!_dirty && _sinceRedrawMs < kRedrawIntervalMs) return;
  _sinceRedrawMs = 0;
  _dirty = false;

  TFT_eSprite& canvas = display.canvas();
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);

  const int16_t gaugeH = 26;
  const int16_t gaugeW = 64;
  const int16_t gaugeY = kMargin;
  drawGauge(canvas, kMargin, gaugeY, gaugeW, gaugeH,
            _battery.valid() ? _battery.percent() : 0);

  canvas.setTextDatum(ML_DATUM);
  if (_battery.valid()) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%u%%  %.2fV", _battery.percent(),
             static_cast<double>(_battery.volts()));
    canvas.drawString(buf, kMargin + gaugeW + 12, gaugeY + gaugeH / 2, 4);
  } else {
    canvas.drawString("no batt sense", kMargin + gaugeW + 12,
                      gaugeY + gaugeH / 2, 2);
  }

  const uint32_t seconds = millis() / 1000;
  char uptime[32];
  snprintf(uptime, sizeof(uptime), "up %lu:%02lu:%02lu",
           static_cast<unsigned long>(seconds / 3600),
           static_cast<unsigned long>((seconds / 60) % 60),
           static_cast<unsigned long>(seconds % 60));
  canvas.setTextDatum(BL_DATUM);
  canvas.drawString(uptime, kMargin, display.height() - kMargin, 4);

  display.present();
}
