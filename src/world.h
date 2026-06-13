#pragma once

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  WORLD_EVENT_TYPE_SPAWN,
  WORLD_EVENT_TYPE_DESPAWN,
  WORLD_EVENT_TYPE_MOVE,
  WORLD_EVENT_TYPE_LOOK,
  WORLD_EVENT_TYPE_JUMP,
  WORLD_EVENT_TYPE_PLACE,
} WorldEventType;

typedef struct {
  WorldEventType type;
  int playerId;
  uint8_t voxelValue;
  int voxelX;
  int voxelY;
  int voxelZ;
  float playerX;
  float playerY;
  float playerZ;
  float playerYaw;
  float playerPitch;
  float moveX;
  float moveY;
  float lookDeltaX;
  float lookDeltaY;
} WorldEvent;

void InitWorld(void);
void QuitWorld(void);
void SendWorldEvent(const WorldEvent* event);
void UpdateWorld(void);
void RenderWorld(int playerId);
