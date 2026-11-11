#pragma once

#include "display/Screen.h"

// Shown while setup() runs: name, version and boot progress
class CBootScreen : public CScreen {

public:
  explicit CBootScreen(CDevice &device) : CScreen(device) {}

  const char* getName() const override { return "Boot"; }
  void draw(Arduino_GFX &gfx) override;
};
