#pragma once

#include "ColorMenu.h"
#include "../world/World.h"

inline constexpr int LOCAL_PLAYER_ID = 0;

void SendSpawnEvent(World& world);
void HandleFrameInput(World& world, ColorMenu& colorMenu);
void DrawHud(const ColorMenu& colorMenu);
