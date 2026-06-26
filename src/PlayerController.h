#pragma once

#include "ColorMenu.h"
#include "world/World.h"

class PlayerController
{
 public:
  static constexpr int kLocalPlayerId = 0;

  void SendSpawnEvent(World& world) const;
  void HandleFrameInput(World& world, ColorMenu& colorMenu) const;
  void DrawHud(const ColorMenu& colorMenu) const;
};
