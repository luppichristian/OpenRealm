#pragma once

#include "WorldConfig.h"

struct VoxelChunkSection
{
  bool dirty = false;
  unsigned int revision = 0;
  uint8_t voxels[CHUNK_SECTION_HEIGHT][CHUNK_SIZE_XZ][CHUNK_SIZE_XZ] = {};
};

struct VoxelChunk
{
  int chunkX = 0;
  int chunkY = 0;
  VoxelChunkSection sections[CHUNK_SECTION_COUNT] = {};
};

struct VoxelRaycastHit
{
  bool hit = false;
  bool placeOnGround = false;
  int voxelX = 0;
  int voxelY = 0;
  int voxelZ = 0;
  int placeX = 0;
  int placeY = 0;
  int placeZ = 0;
};

struct CollisionMoveResult
{
  bool collided = false;
  bool hitFloor = false;
  float movedDelta = 0.0f;
};

struct PlayerFrameInput
{
  float moveX = 0.0f;
  float moveY = 0.0f;
  float lookDeltaX = 0.0f;
  float lookDeltaY = 0.0f;
  bool jumpRequested = false;
  bool placeRequested = false;
  uint8_t placeVoxelValue = 0;
};

struct PlayerState
{
  int id = 0;
  bool active = false;
  bool isGrounded = false;
  Vector3 position = {};
  float yaw = 0.0f;
  float pitch = 0.0f;
  float footstepTimer = 0.0f;
  float verticalVelocity = 0.0f;
  bool jumpedSinceGrounded = false;
  bool playFootstepSound = false;
  bool playLandingSound = false;
  PlayerFrameInput frameInput = {};
};
