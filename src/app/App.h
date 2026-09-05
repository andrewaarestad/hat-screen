#pragma once

#include "app/Scene.h"
#include "display/Display.h"
#include "power/Battery.h"
#include "scenes/MarqueeScene.h"
#include "scenes/StatusScene.h"

// Wires the hardware to the scenes and paces the frame loop.
class App {
 public:
  bool begin();
  void loop();

 private:
  void pollButton();
  void nextScene();
  void applyPowerPolicy();
  Scene& current() { return *_scenes[_sceneIndex]; }

  Display _display;
  Battery _battery;

  MarqueeScene _marquee{"HELLO FROM THE HAT"};
  StatusScene _status{_battery};
  Scene* _scenes[2] = {&_marquee, &_status};
  uint8_t _sceneIndex = 0;

  uint32_t _lastFrameMs = 0;
  uint32_t _sceneElapsedMs = 0;

  // Button debounce state.
  bool _buttonWasDown = false;
  uint32_t _buttonChangedMs = 0;

  bool _dimmedForLowBattery = false;
};
