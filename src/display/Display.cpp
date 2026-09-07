#include "display/Display.h"

namespace {

constexpr uint8_t kBacklightChannel = 0;
constexpr uint32_t kBacklightFreqHz = 5000;
constexpr uint8_t kBacklightResBits = 8;

// Offsets from the far edge of the GRAM, used by the mirrored rotations.
constexpr int16_t kColOffsetMirrored =
    GRAM_WIDTH - PANEL_WIDTH - PANEL_COL_OFFSET;
constexpr int16_t kRowOffsetMirrored =
    GRAM_HEIGHT - PANEL_HEIGHT - PANEL_ROW_OFFSET;

// ST7789 commands used directly, rather than via TFT_eSPI's internal defines.
constexpr uint8_t kCmdDisplayOff = 0x28;
constexpr uint8_t kCmdSleepIn = 0x10;

void backlightAttach(int8_t pin) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(pin, kBacklightFreqHz, kBacklightResBits);
#else
  ledcSetup(kBacklightChannel, kBacklightFreqHz, kBacklightResBits);
  ledcAttachPin(pin, kBacklightChannel);
#endif
}

void backlightDetach(int8_t pin) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcDetach(pin);
#else
  (void)pin;
  ledcDetachPin(pin);
#endif
}

void backlightWrite(int8_t pin, uint8_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, duty);
#else
  (void)pin;
  ledcWrite(kBacklightChannel, duty);
#endif
}

}  // namespace

bool Display::begin() {
  backlightAttach(PIN_TFT_BL);
  // Stay dark until the first frame is ready, otherwise the panel shows GRAM
  // garbage for a few hundred milliseconds at power-on.
  setBacklight(0);

  _tft.init();
  _tft.fillScreen(TFT_BLACK);

  setRotation(DISPLAY_DEFAULT_ROTATION);
  if (!_canvasReady) return false;

  _canvas.fillSprite(TFT_BLACK);
  present();
  setBacklight(BACKLIGHT_DEFAULT);
  return true;
}

// The panel is a window into the controller's 240x320 GRAM. In rotation 0 that
// window starts at (PANEL_COL_OFFSET, PANEL_ROW_OFFSET). Rotating swaps the
// axes and mirrors one of them, so the offsets swap and flip to match.
//
// If your panel's window turns out not to be centred, only rotations 0/1 use
// the raw offsets -- 2/3 use the mirrored ones, which is why both are computed
// rather than hard-coded.
void Display::applyViewport(uint8_t rotation) {
  _tft.setRotation(rotation);  // this resets any previous viewport

  switch (rotation & 0x03) {
    case 0:
      _tft.setViewport(PANEL_COL_OFFSET, PANEL_ROW_OFFSET, PANEL_WIDTH,
                       PANEL_HEIGHT);
      _width = PANEL_WIDTH;
      _height = PANEL_HEIGHT;
      break;
    case 1:
      _tft.setViewport(PANEL_ROW_OFFSET, PANEL_COL_OFFSET, PANEL_HEIGHT,
                       PANEL_WIDTH);
      _width = PANEL_HEIGHT;
      _height = PANEL_WIDTH;
      break;
    case 2:
      _tft.setViewport(kColOffsetMirrored, kRowOffsetMirrored, PANEL_WIDTH,
                       PANEL_HEIGHT);
      _width = PANEL_WIDTH;
      _height = PANEL_HEIGHT;
      break;
    default:
      _tft.setViewport(kRowOffsetMirrored, kColOffsetMirrored, PANEL_HEIGHT,
                       PANEL_WIDTH);
      _width = PANEL_HEIGHT;
      _height = PANEL_WIDTH;
      break;
  }
}

void Display::setRotation(uint8_t rotation) {
  _rotation = rotation & 0x03;
  applyViewport(_rotation);

  if (_canvasReady) {
    _canvas.deleteSprite();
    _canvasReady = false;
  }

  // 16 bpp full-frame canvas: 284 * 76 * 2 = ~43 kB, comfortable on the C3's
  // ~320 kB of usable heap. Drop to 8 bpp here if you need that memory back.
  _canvas.setColorDepth(16);
  _canvasReady = _canvas.createSprite(_width, _height) != nullptr;
  if (_canvasReady) {
    _canvas.fillSprite(TFT_BLACK);
  }
}

void Display::setBacklight(uint8_t duty) {
  _backlight = duty;
  backlightWrite(PIN_TFT_BL, duty);
}

void Display::present() {
  if (!_canvasReady) return;
  _canvas.pushSprite(0, 0);
}

void Display::powerOff() {
  setBacklight(0);
  _tft.writecommand(kCmdDisplayOff);
  _tft.writecommand(kCmdSleepIn);

  // Park the backlight pin low. GPIO21 is not an RTC GPIO, so it goes
  // high-impedance in deep sleep and cannot be held -- if your panel's BLK
  // input floats high enough to glow, fit a 100k pulldown on it.
  backlightDetach(PIN_TFT_BL);
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, LOW);
}

void Display::drawCalibrationFrame() {
  if (!_canvasReady) return;
  _canvas.fillSprite(TFT_BLACK);
  _canvas.drawRect(0, 0, _width, _height, TFT_WHITE);
  // Corner ticks make it obvious which edge is being clipped.
  _canvas.fillRect(0, 0, 8, 8, TFT_RED);                    // top-left
  _canvas.fillRect(_width - 8, 0, 8, 8, TFT_GREEN);         // top-right
  _canvas.fillRect(0, _height - 8, 8, 8, TFT_BLUE);         // bottom-left
  _canvas.fillRect(_width - 8, _height - 8, 8, 8, TFT_YELLOW);
  _canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  _canvas.setTextDatum(MC_DATUM);
  _canvas.drawString(String(_width) + "x" + String(_height), _width / 2,
                     _height / 2, 2);
  present();
}
