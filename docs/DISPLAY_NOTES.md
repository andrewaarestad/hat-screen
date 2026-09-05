# Display notes: driving a 76x284 ST7789

## The offset problem

The ST7789 has a fixed 240x320 pixel framebuffer (GRAM). Panels smaller than
that expose a window into it, and the controller has no idea where that window
sits — the panel maker decides at bonding time. Writing to pixel (0, 0) puts
data at GRAM (0, 0), which on a 76x284 panel is usually *not* the top-left pixel
you can see.

Libraries handle this with hard-coded offset tables for panel sizes they know
about. TFT_eSPI knows 240x240, 135x240, 172x320 and 170x320. It does not know
76x284, so it would use offsets of 0 and every drawing operation would be
shifted and clipped.

## How this firmware handles it

Rather than patching the library, `platformio.ini` configures TFT_eSPI for the
controller's **full** 240x320 GRAM, and `Display::applyViewport()` then calls
`setViewport()` to restrict drawing to the visible window:

```cpp
_tft.setRotation(rotation);   // resets any previous viewport
_tft.setViewport(PANEL_ROW_OFFSET, PANEL_COL_OFFSET, PANEL_HEIGHT, PANEL_WIDTH);
```

Inside a viewport, TFT_eSPI treats (0, 0) as the viewport's top-left corner and
clips anything outside it, so the rest of the firmware can draw in plain panel
coordinates and never think about GRAM again.

`setRotation()` resets the viewport, so it must always be re-applied afterwards —
which is why the two calls live together in one function.

## Rotation and offsets

Rotating swaps the axes, so the offsets swap too, and the mirrored rotations (2
and 3) measure from the opposite edge of the GRAM:

| Rotation | Size | Viewport origin |
| --- | --- | --- |
| 0 (portrait) | 76 x 284 | (col, row) |
| 1 (landscape) | 284 x 76 | (row, col) |
| 2 (portrait, flipped) | 76 x 284 | (240−76−col, 320−284−row) |
| 3 (landscape, flipped) | 284 x 76 | (320−284−row, 240−76−col) |

If the window really is centred, all four collapse to the same 82/18 numbers.
The code computes the mirrored variants anyway so that an off-centre panel still
works in every rotation.

Default is rotation 1: long axis horizontal, which is what a hat brim wants.

## Calibrating

The 82/18 offsets are a *centred-window assumption*, not a measured value from
this specific panel. Verify them before building anything on top:

```sh
pio run -e xiao_esp32c3_calibrate -t upload
```

The firmware draws a 1 px white frame around the panel with coloured corner
squares (red = top-left, green = top-right, blue = bottom-left, yellow =
bottom-right) and then halts.

- **All four edges visible, corners in the corners** → offsets are right.
- **Missing left edge, dark band on the right** → decrease `PANEL_COL_OFFSET`.
- **Missing top edge, dark band at the bottom** → decrease `PANEL_ROW_OFFSET`.
- **Nothing at all, or noise** → wiring or SPI speed, not offsets. Check `DC`
  and `RST` first, then drop `SPI_FREQUENCY`.

Adjust `include/board_config.h` and re-flash until the frame sits exactly on the
glass edge.

## Colour

Two flags in `platformio.ini` cover the usual surprises:

- `TFT_INVERSION_ON` — most ST7789 modules need it. Remove it if the display
  comes up looking like a photo negative.
- `TFT_RGB_ORDER=TFT_BGR` — change to `TFT_RGB` if red and blue are swapped.

## Why a full-frame sprite

Drawing a scrolling marquee straight to the panel means clearing and redrawing
text every frame, which tears visibly. `Display` instead owns a 16 bpp
`TFT_eSprite` the size of the panel; scenes render into it and `present()`
blits it in one SPI transaction.

Cost is 284 × 76 × 2 = ~43 kB of the C3's ~320 kB usable heap. If you need that
memory back, `setColorDepth(8)` in `Display::setRotation()` halves it at the
cost of a 256-colour palette.
