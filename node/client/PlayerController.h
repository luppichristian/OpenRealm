#pragma once

#include "ColorMenu.h"
#include "../world/World.h"

inline constexpr int LOCAL_PLAYER_ID = 0;

struct PlayerInputOptions
{
  float mouseSensitivity = 1.0f;
  bool invertMouseY = false;
};

void SendSpawnEvent(World& world);
void HandleFrameInput(World& world, ColorMenu& colorMenu, const PlayerInputOptions& inputOptions);
void DrawHud(const ColorMenu& colorMenu);
