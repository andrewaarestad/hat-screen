#pragma once

#include <TFT_eSPI.h>

#include "board_config.h"

// Owns the ST7789 panel and an off-screen canvas.
//
// Two things this hides from the rest of the firmware:
//
//  1. GRAM offsets. TFT_eSPI is configured for the controller's full 240x320
//     framebuffer; setRotation() below re-applies a viewport so that (0,0) is
//     the top-left visible pixel of the 76x284 glass.
//
//  2. Tearing. A scrolling marquee redrawn directly on the panel flickers, so
//     scenes draw into a full-frame sprite and we blit it once per frame.
class Display {
 public:
  bool begin();

  // Recomputes the panel viewport and reallocates the canvas.
  void setRotation(uint8_t rotation);

  // 0-255. Uses LEDC PWM; 0 turns the backlight fully off.
  void setBacklight(uint8_t duty);
  uint8_t backlight() const { return _backlight; }

  // Scenes draw here, in panel coordinates.
  TFT_eSprite& canvas() { return _canvas; }

  // Blit the canvas to the panel.
  void present();

  int16_t width() const { return _width; }
  int16_t height() const { return _height; }

  // Draws a 1 px frame on the outermost pixels of the panel plus corner marks.
  // If any edge is clipped or floating, PANEL_COL_OFFSET / PANEL_ROW_OFFSET in
  // board_config.h are wrong.
  void drawCalibrationFrame();

  TFT_eSPI& raw() { return _tft; }

 private:
  void applyViewport(uint8_t rotation);

  TFT_eSPI _tft;
  TFT_eSprite _canvas{&_tft};
  int16_t _width = PANEL_HEIGHT;
  int16_t _height = PANEL_WIDTH;
  uint8_t _rotation = DISPLAY_DEFAULT_ROTATION;
  uint8_t _backlight = 0;
  bool _canvasReady = false;
};
