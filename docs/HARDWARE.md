# Hardware notes

## Pin map

XIAO ESP32C3 silkscreen → ESP32-C3 GPIO → function.

| XIAO pin | GPIO | Connects to | Notes |
| --- | --- | --- | --- |
| D8 / SCK | 8 | Display `SCL` / `SCK` | Strapping pin; only ever an output here |
| D10 / MOSI | 10 | Display `SDA` / `MOSI` | |
| D1 | 3 | Display `CS` | |
| D2 | 4 | Display `DC` / `RS` | |
| D3 | 5 | Display `RES` / `RST` | |
| D6 / TX | 21 | Display `BLK` (backlight) | PWM dimming, see caveat below |
| D0 / A0 | 2 | Battery divider tap | ADC1; strapping pin, held high by divider |
| D7 / RX | 20 | Button to GND | Internal pull-up, active LOW |
| D4 / SDA | 6 | *free* | Reserved for I2C (IMU, sensors) |
| D5 / SCL | 7 | *free* | Reserved for I2C |
| 3V3 | — | Display `VCC` | |
| GND | — | Display `GND` | |

The display's `MISO` is unused; leave it unconnected (`TFT_MISO=-1`).

The pin numbers appear twice — as `-D TFT_*` flags in `platformio.ini` (TFT_eSPI
resolves them at compile time) and as constants in `include/board_config.h`.
Change both together.

### Backlight pin caveat

`D6` is also UART0 TX, which the ROM bootloader uses for its boot log. The
backlight therefore flickers for roughly 200 ms at power-on. This is cosmetic.
If it bothers you, either move the backlight to `D5` (giving up the I2C pair) or
disable the ROM log by strapping `GPIO8` per the ESP32-C3 datasheet.

## Power

The XIAO ESP32C3 has an on-board charger — solder the cell to the `BAT+` /
`BAT-` pads on the underside and it charges whenever USB-C is connected, with a
charge-status LED. Check Seeed's XIAO ESP32C3 wiki for the charge current before
choosing a cell; small cells want a low charge rate.

Rough current draw, for sizing the pack:

| State | Approximate draw |
| --- | --- |
| ESP32-C3 active, radios off | 20–25 mA |
| Backlight at full brightness | 40–80 mA (panel dependent) |
| WiFi TX bursts | +100–200 mA peak |

The backlight dominates, which is why `BACKLIGHT_DEFAULT` is 140/255 rather than
full, and why the firmware dims to 40/255 below 3.4 V. Measure your own panel
before trusting any of these numbers.

**Not yet implemented:** a hard low-voltage cutoff. The XIAO's charger has no
load disconnect, so a cell left running will discharge below its safe floor.
Either use a protected cell or add a cutoff before leaving this unattended.

## Battery sensing

Unlike the XIAO nRF52840, the ESP32C3 variant has no on-board battery divider,
so battery voltage has to be brought out yourself:

```
BAT+ ──┬── 100k ──┬── 100k ── GND
       │          │
    (to XIAO)     └── D0 / GPIO2  (ADC)
```

A fully charged 4.2 V cell reads ~2.1 V at the tap, inside the ESP32-C3 ADC's
range at 11 dB attenuation. `GPIO2` is a strapping pin that must be high at
boot; the divider holds it around 2.1 V, so this is safe.

If you skip the divider, set `VBAT_SENSE_FITTED = false` in
`include/board_config.h` — otherwise the floating pin reads as a flat cell and
the firmware permanently dims the backlight.

The ESP32-C3 ADC is noisy and not factory-calibrated to better than a few
percent. `Battery` oversamples and smooths, but treat the percentage as a rough
indication rather than a fuel gauge — especially under load, when the cell sags.

## Assembly notes for wearing it

- SPI at 40 MHz over long, thin wire is unreliable. If the panel glitches or
  shows noise, drop `SPI_FREQUENCY` to 27 MHz or 20 MHz in `platformio.ini`.
- Strain-relieve the display ribbon/flex; it is the first thing to fail when a
  hat is taken on and off.
- Keep the LiPo out of the crown where it will be squashed or punctured. A
  puncture in a cell worn on your head is the worst failure mode in this project
  by a wide margin.
- The XIAO's antenna is at the USB end of the board; don't bury it in metal
  trim or under a foil-lined brim if you want WiFi/BLE later.
