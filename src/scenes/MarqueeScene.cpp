#include "scenes/MarqueeScene.h"

void MarqueeScene::setText(const String& text) {
  _text = text;
  _measured = false;
}

void MarqueeScene::measure(Display& display) {
  TFT_eSprite& canvas = display.canvas();
  canvas.setTextFont(_font);
  canvas.setTextSize(1);
  _textWidth = canvas.textWidth(_text, _font);
  _measured = true;
}

void MarqueeScene::onEnter(Display& display) {
  measure(display);
  // Start just off the right edge so the message scrolls in rather than
  // appearing mid-sentence.
  _offset = static_cast<float>(display.width());
}

void MarqueeScene::update(Display& display, uint32_t dtMs) {
  TFT_eSprite& canvas = display.canvas();
  if (!_measured) measure(display);

  _offset -= _pixelsPerSecond * (dtMs / 1000.0f);
  const float period = static_cast<float>(_textWidth + _gap);
  if (period > 0.0f) {
    while (_offset <= -period) _offset += period;
  }

  canvas.fillSprite(_bg);
  canvas.setTextColor(_fg, _bg);
  canvas.setTextDatum(ML_DATUM);
  canvas.setTextFont(_font);

  const int16_t y = display.height() / 2;
  const int16_t x = static_cast<int16_t>(_offset);
  canvas.drawString(_text, x, y);
  // Second copy trailing the first, so the loop has no visible seam.
  if (period > 0.0f && x + _textWidth < display.width()) {
    canvas.drawString(_text, x + static_cast<int16_t>(period), y);
  }

  display.present();
}
