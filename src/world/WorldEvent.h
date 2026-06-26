#pragma once

#include "../Base.h"

enum class WorldEventType
{
  Spawn,
  Despawn,
  Move,
  Look,
  Jump,
  Place,
};

struct WorldEvent
{
  WorldEventType type = WorldEventType::Move;
  int playerId = 0;
  uint8_t voxelValue = 0;
  int voxelX = 0;
  int voxelY = 0;
  int voxelZ = 0;
  float playerX = 0.0f;
  float playerY = 0.0f;
  float playerZ = 0.0f;
  float playerYaw = 0.0f;
  float playerPitch = 0.0f;
  float moveX = 0.0f;
  float moveY = 0.0f;
  float lookDeltaX = 0.0f;
  float lookDeltaY = 0.0f;
};
