#pragma once

#include <stdint.h>

class Battery;
class Display;

// Battery-driven power policy: dims the backlight when the cell gets low, and
// puts the board into deep sleep before the cell is damaged.
//
// The XIAO's charger has no load disconnect, so nothing in hardware stops a
// flat cell from being drained past its floor. Deep sleep is the closest thing
// available -- it drops the board to tens of microamps, which a LiPo can hold
// for months, and the charger keeps working while asleep so the hat recovers on
// its own once it is plugged in.
class PowerManager {
 public:
  enum class State : uint8_t {
    Normal,    // running at full brightness
    Low,       // dimmed, still running
    Shutdown,  // notice on screen, deep sleep imminent
  };

  // Call before the display is initialised, with a battery that has already
  // been begun. If this boot is a wake from a low-voltage sleep and the cell
  // has not recovered, this does NOT return -- it goes straight back to sleep
  // without ever lighting the panel.
  static void guardBoot(Battery& battery);

  void begin(Display& display, Battery& battery);

  // Call once per frame. Returns false once the shutdown sequence has taken
  // over the display; the caller must stop drawing scenes.
  bool update(uint32_t dtMs);

  State state() const { return _state; }

 private:
  void enterShutdown();
  void drawShutdownNotice();
  void powerDown();

  Display* _display = nullptr;
  Battery* _battery = nullptr;
  State _state = State::Normal;
  uint32_t _belowCutoffMs = 0;
  uint32_t _shutdownElapsedMs = 0;
  bool _noticeDrawn = false;
};
