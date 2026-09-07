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

## Low-voltage cutoff

The XIAO's charger has no load disconnect, so nothing in hardware stops a flat
cell from being drained past its floor. The firmware handles it instead: below
3.20 V it shows a notice for 5 s and then deep-sleeps, dropping the board to
tens of microamps — a level a LiPo can sit at for months.

Thresholds live in `include/board_config.h`:

| Constant | Default | Effect |
| --- | --- | --- |
| `VBAT_LOW_VOLTS` | 3.40 V | Backlight dims to 40/255 |
| `VBAT_CUTOFF_VOLTS` | 3.20 V | Shutdown sequence starts |
| `VBAT_RESUME_VOLTS` | 3.70 V | Wakes back up above this |
| `VBAT_PLAUSIBLE_MIN_VOLTS` | 2.50 V | Below this, the cutoff is disarmed |
| `VBAT_CUTOFF_CONFIRM_MS` | 8000 ms | How long the reading must stay low |
| `VBAT_SLEEP_RECHECK_S` | 300 s | How often sleep wakes to re-check |

The cutoff is at 3.20 V rather than a LiPo's true 3.0 V floor because these
readings are taken under load, through an ADC good to only a few percent. The
margin costs a few minutes of runtime and buys not destroying the cell.

**Recovery is automatic.** The charger is hardware and keeps working while the
MCU sleeps, so the board wakes every 5 minutes, re-checks, and resumes once the
cell is above 3.70 V. Nothing needs to be pressed. On a wake the check runs
*before* the panel is initialised, so a failed recheck never lights the display.

Two guards worth knowing about:

- **A reading below 2.50 V disarms the cutoff entirely.** A 1S LiPo that low is
  already destroyed and the XIAO's regulator would have browned out long before,
  so such a reading means no divider or no battery — not a flat cell. Without
  this, flashing a board with no cell fitted would put it straight to sleep.
- **The reading must stay low for 8 s continuously.** A single sagging sample
  during a WiFi burst or a backlight step does not trigger a shutdown.

### Waking on a button press

Not currently possible with this pin map. Only `GPIO0`–`GPIO5` are RTC-capable
on the ESP32-C3, and the button is on `D7` / `GPIO20`. To add it, move the
button to `D1`, `D2` or `D3` — relocating whichever TFT signal is there to `D7`
— and arm `esp_deep_sleep_enable_gpio_wakeup()` alongside the timer.

### Backlight during deep sleep

`Display::powerOff()` drives `BLK` low before sleeping, but `GPIO21` is not an
RTC GPIO so it cannot be *held* — it goes high-impedance once asleep. If your
panel's backlight glows faintly in deep sleep, fit a 100 kΩ pulldown on `BLK`.

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
