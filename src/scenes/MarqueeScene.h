#pragma once

#include <Arduino.h>

#include "app/Scene.h"

// Horizontally scrolling text -- the obvious use for a 284x76 strip on a hat.
//
// The message is drawn twice, one message-width apart, so it wraps seamlessly
// instead of clearing and restarting when it runs off the left edge.
class MarqueeScene : public Scene {
 public:
  explicit MarqueeScene(const String& text) : _text(text) {}

  const char* name() const override { return "marquee"; }

  void setText(const String& text);
  void setSpeed(float pixelsPerSecond) { _pixelsPerSecond = pixelsPerSecond; }
  void setColors(uint16_t fg, uint16_t bg) {
    _fg = fg;
    _bg = bg;
  }

  void onEnter(Display& display) override;
  void update(Display& display, uint32_t dtMs) override;

 private:
  void measure(Display& display);

  String _text;
  float _offset = 0.0f;
  float _pixelsPerSecond = 70.0f;
  int16_t _textWidth = 0;
  int16_t _gap = 48;
  uint8_t _font = 4;
  uint16_t _fg = TFT_WHITE;
  uint16_t _bg = TFT_BLACK;
  bool _measured = false;
};
