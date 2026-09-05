#pragma once

#include <stdint.h>

#include "display/Display.h"

// One thing the hat can show. Scenes own their animation state and are asked to
// redraw the whole canvas each frame -- there is no dirty-rectangle tracking,
// because a 284x76 sprite blit is cheap enough not to need it.
class Scene {
 public:
  virtual ~Scene() = default;

  // Shown in the scene switcher and in serial logs.
  virtual const char* name() const = 0;

  // Called when this scene becomes visible. Reset animation state here.
  virtual void onEnter(Display& display) { (void)display; }
  virtual void onExit(Display& display) { (void)display; }

  // dtMs is the time since the previous update, so animation speed stays
  // constant if a frame runs long.
  virtual void update(Display& display, uint32_t dtMs) = 0;

  // Return true if the scene consumed the press; otherwise the app advances to
  // the next scene.
  virtual bool onButtonPress() { return false; }
};
