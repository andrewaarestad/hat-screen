# hat-screen

Firmware for a wearable display: a long, skinny TFT mounted on a hat, driven by
a XIAO ESP32C3 running off a single LiPo cell.

## Hardware

| Part | Notes |
| --- | --- |
| Seeed Studio XIAO ESP32C3 | RISC-V, WiFi/BLE, USB-C, on-board LiPo charger |
| 2.25" 76x284 ST7789 TFT | SPI, "long strip" panel, 4-wire SPI + backlight |
| 3.7 V LiPo, 1S | Soldered to the XIAO's `BAT+` / `BAT-` pads |
| Momentary button | Scene switching, wired to `D7` and ground |
| 2x 100 kΩ resistors | Battery-voltage divider (optional, see below) |

Schematic: [docs/schematic/](docs/schematic/) ([PDF](docs/schematic/hat-screen.pdf)). Full pin map, power budget and
assembly notes: [docs/HARDWARE.md](docs/HARDWARE.md).

## Quick start

```sh
pip install platformio        # or: brew install platformio
pio run                       # build
pio run -t upload             # flash over USB-C
pio device monitor            # 115200 baud, USB CDC
```

If the XIAO does not show up as a serial port, hold `BOOT`, tap `RESET`, release
`BOOT` to force the ROM bootloader, then upload.

## First power-up: check the panel offsets

This is the one step that most often needs adjusting, and it is worth doing
before anything else looks wrong for mysterious reasons.

The ST7789 controller always has a 240x320 framebuffer, but this panel is only
76x284 pixels — the glass shows a *window* into that framebuffer. The firmware
assumes the window is centred (offsets of 82 and 18). If your panel is offset
differently, everything you draw is shifted and clipped.

To check:

```sh
pio run -e xiao_esp32c3_calibrate -t upload
```

You should see a 1 px white frame exactly on the panel's outer edge, with a
coloured square in each corner. If an edge is missing or there is a dark band,
adjust `PANEL_COL_OFFSET` / `PANEL_ROW_OFFSET` in
[`include/board_config.h`](include/board_config.h) and re-flash. Details in
[docs/DISPLAY_NOTES.md](docs/DISPLAY_NOTES.md).

## Layout

```
platformio.ini           Build config; all TFT_eSPI setup lives here
include/board_config.h   Pin map, panel geometry, timing constants
src/main.cpp             setup()/loop()
src/app/                 App (frame loop, input, scene switching) + Scene base
src/display/             Display: panel init, GRAM offsets, off-screen canvas
src/power/               Battery sampling; PowerManager dimming and cutoff
src/scenes/              MarqueeScene (scrolling text), StatusScene (battery)
docs/                    Wiring and display notes
docs/schematic/          KiCad schematic (no PCB), plus the pin-map check
```

Scenes draw into a full-frame 16 bpp sprite (~43 kB) which is blitted once per
frame, so scrolling does not tear. Press the button to advance to the next
scene; scenes also auto-cycle every 15 s (`SCENE_AUTO_CYCLE_MS`).

## Adding a scene

Subclass `Scene` (`src/app/Scene.h`), implement `name()` and `update()`, then
add an instance to the `_scenes` array in `src/app/App.h`:

```cpp
class ClockScene : public Scene {
 public:
  const char* name() const override { return "clock"; }
  void update(Display& display, uint32_t dtMs) override {
    TFT_eSprite& canvas = display.canvas();
    canvas.fillSprite(TFT_BLACK);
    // ...draw in panel coordinates, 0,0 is the top-left visible pixel...
    display.present();
  }
};
```

## Roadmap

Things this scaffold deliberately leaves as stubs, roughly in the order they
make sense to build:

- [ ] Verify the panel offsets on real hardware and fix the constants
- [ ] BLE or WiFi captive portal to set the marquee text from a phone
- [ ] Persist text and brightness in NVS across power cycles
- [ ] Bitmap / sprite-sheet animation scene
- [x] Low-voltage cutoff to protect the cell (see docs/HARDWARE.md)
- [ ] Light sleep between frames and a proper power budget measurement
- [ ] Move the button to an RTC GPIO so it can wake the hat from deep sleep
- [ ] IMU on the free I2C pair (D4/D5) for tilt-reactive effects
