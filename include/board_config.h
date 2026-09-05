#pragma once

#include <stdint.h>

// -----------------------------------------------------------------------------
// Board: Seeed Studio XIAO ESP32C3
// Display: 2.25" 76x284 ST7789 SPI TFT ("long strip" panel)
// -----------------------------------------------------------------------------
// XIAO silkscreen -> ESP32-C3 GPIO
//   D0/A0 = 2    D1/A1 = 3    D2/A2 = 4    D3/A3 = 5
//   D4/SDA = 6   D5/SCL = 7   D6/TX = 21   D7/RX = 20
//   D8/SCK = 8   D9/MISO = 9  D10/MOSI = 10
//
// The TFT pins below are duplicated as -D flags in platformio.ini because
// TFT_eSPI resolves them at compile time. Keep the two in sync.
// -----------------------------------------------------------------------------

// ---- Display (SPI) ----------------------------------------------------------
static constexpr int8_t PIN_TFT_SCLK = 8;   // D8
static constexpr int8_t PIN_TFT_MOSI = 10;  // D10
static constexpr int8_t PIN_TFT_CS   = 3;   // D1
static constexpr int8_t PIN_TFT_DC   = 4;   // D2
static constexpr int8_t PIN_TFT_RST  = 5;   // D3

// Backlight, driven by LEDC PWM so we can dim it.
// GPIO21 is also UART0 TX, so the ROM bootloader briefly toggles it and the
// backlight flickers for ~200 ms at power-on. Harmless; see docs/HARDWARE.md
// if you want to silence it. Using D6 here keeps D4/D5 free as an I2C pair.
static constexpr int8_t PIN_TFT_BL = 21;  // D6

// ---- Inputs -----------------------------------------------------------------
// Momentary button to ground, using the internal pull-up (active LOW).
static constexpr int8_t PIN_BUTTON = 20;  // D7

// ---- Battery sense ----------------------------------------------------------
// The XIAO ESP32C3 has no on-board battery divider (unlike the nRF52840 XIAO),
// so this expects an external 2x 100k divider from BAT+ to GND with the tap on
// D0. GPIO2 is an ESP32-C3 strapping pin that must be high at boot -- the
// divider holds it near 2.1 V, which reads as high, so this is safe.
static constexpr int8_t PIN_VBAT_SENSE = 2;  // D0

// Ratio of actual battery voltage to the voltage at the ADC pin.
static constexpr float VBAT_DIVIDER_RATIO = 2.0f;

// Set to false if you have not fitted the divider; the UI then hides battery
// state instead of reporting a floating pin as a dead cell.
static constexpr bool VBAT_SENSE_FITTED = true;

// ---- Panel geometry ---------------------------------------------------------
// The ST7789 always has a 240x320 GRAM. This panel only shows a 76x284 window
// into it, so every drawing operation has to be offset. We assume the window is
// centred: (240-76)/2 = 82 and (320-284)/2 = 18.
//
// These offsets are the single most likely thing to be wrong on first power-up.
// Build with HAT_CALIBRATE defined to draw a 1 px border around the panel and
// nudge the numbers until the frame sits exactly on the glass edge.
static constexpr int16_t GRAM_WIDTH  = 240;
static constexpr int16_t GRAM_HEIGHT = 320;
static constexpr int16_t PANEL_WIDTH  = 76;
static constexpr int16_t PANEL_HEIGHT = 284;
static constexpr int16_t PANEL_COL_OFFSET = (GRAM_WIDTH - PANEL_WIDTH) / 2;    // 82
static constexpr int16_t PANEL_ROW_OFFSET = (GRAM_HEIGHT - PANEL_HEIGHT) / 2;  // 18

// 0 = portrait (76 wide x 284 tall), 1 = landscape (284 x 76).
// A hat brim wants landscape: long axis horizontal, text reading left to right.
static constexpr uint8_t DISPLAY_DEFAULT_ROTATION = 1;

// ---- Timing / power ---------------------------------------------------------
static constexpr uint16_t TARGET_FPS = 30;
static constexpr uint16_t FRAME_INTERVAL_MS = 1000 / TARGET_FPS;

// Backlight duty (0-255). Full brightness on a hat is both blinding and the
// single largest current draw, so default to something wearable.
static constexpr uint8_t BACKLIGHT_DEFAULT = 140;

// Cycle to the next scene automatically after this long. 0 disables.
static constexpr uint32_t SCENE_AUTO_CYCLE_MS = 15000;
