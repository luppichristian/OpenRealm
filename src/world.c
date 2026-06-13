#include "world.h"

#include <math.h>
#include <string.h>

#include <raymath.h>

#define CHUNK_SIZE_XZ             16
#define CHUNK_MAX_HEIGHT          1020
#define MAX_VOXEL_CHUNKS          64
#define MAX_WORLD_EVENTS          64
#define MAX_PLAYERS               32
#define WORLD_UP                  ((Vector3) {0.0f, 0.0f, 1.0f})
#define MOUSE_SENSITIVITY         0.003f
#define MOVE_SPEED                6.0f
#define FOOTSTEP_INTERVAL_SECONDS 0.45f
#define FOOTSTEP_VOLUME           0.55f
#define FOOTSTEP_PITCH            1.0f
#define LANDING_VOLUME            0.65f
#define LANDING_PITCH             1.0f
#define GRAVITY                   24.0f
#define JUMP_VELOCITY             9.5f
#define GROUND_HEIGHT             0.0f
#define EYE_HEIGHT                3.0f
#define PLAYER_RADIUS             0.45f
#define PLAYER_HEIGHT             3.4f
#define BLOCK_GRID_SIZE           0.375f
#define VOXEL_EDIT_DISTANCE       14.0f
#define MAX_ABS_PITCH             (PI / 2.0f - 0.01f)
#define VERTICAL_FOV_DEGREES      90.0f
#define VERTICAL_FOV_RADIANS      (VERTICAL_FOV_DEGREES * DEG2RAD)

typedef struct {
  int chunkX;
  int chunkY;
  uint8_t voxels[CHUNK_MAX_HEIGHT][CHUNK_SIZE_XZ][CHUNK_SIZE_XZ];
} VoxelChunk;

typedef struct {
  bool hit;
  bool placeOnGround;
  int voxelX;
  int voxelY;
  int voxelZ;
  int placeX;
  int placeY;
  int placeZ;
} VoxelRaycastHit;

typedef struct {
  bool collided;
  float movedDelta;
} CollisionMoveResult;

typedef struct {
  float moveX;
  float moveY;
  float lookDeltaX;
  float lookDeltaY;
  bool jumpRequested;
  bool placeRequested;
  uint8_t placeVoxelValue;
} PlayerFrameInput;

typedef struct {
  int id;
  bool active;
  Vector3 position;
  float yaw;
  float pitch;
  float footstepTimer;
  float verticalVelocity;
  bool jumpedSinceGrounded;
  PlayerFrameInput frameInput;
} PlayerState;

typedef struct {
  bool initialized;
  Shader floorShader;
  Shader voxelShader;
  Sound footstepSound;
  Sound landingSound;
  Model voxelModel;
  int floorCameraPositionLoc;
  int floorCameraForwardLoc;
  int floorCameraRightLoc;
  int floorCameraUpLoc;
  int floorResolutionLoc;
  int floorVerticalFovLoc;
  int floorAspectRatioLoc;
  int floorTimeSecondsLoc;
  int floorBlockGridSizeLoc;
  int floorChunkGridSizeLoc;
  int voxelCameraPositionLoc;
  int voxelTimeSecondsLoc;
  int voxelBlockColorLoc;
  int voxelFaceOcclusionALoc;
  int voxelFaceOcclusionBLoc;
} WorldAssets;

typedef struct {
  bool initialized;
  VoxelChunk chunks[MAX_VOXEL_CHUNKS];
  int chunkCount;
  PlayerState players[MAX_PLAYERS];
  WorldAssets assets;
  WorldEvent eventQueue[MAX_WORLD_EVENTS];
  int eventCount;
} WorldState;

static WorldState world = {0};

static void EnsureRenderAssetsInitialized(void) {
  if (world.assets.initialized) return;

  world.assets.floorShader = LoadShader(0, "assets/shaders/infinite_floor.fs");
  world.assets.voxelShader = LoadShader("assets/shaders/voxel.vs", "assets/shaders/voxel.fs");
  world.assets.footstepSound = LoadSound("assets/sounds/footstep.wav");
  world.assets.landingSound = LoadSound("assets/sounds/footstep.wav");
  world.assets.voxelModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
  world.assets.floorCameraPositionLoc = GetShaderLocation(world.assets.floorShader, "cameraPosition");
  world.assets.floorCameraForwardLoc = GetShaderLocation(world.assets.floorShader, "cameraForward");
  world.assets.floorCameraRightLoc = GetShaderLocation(world.assets.floorShader, "cameraRight");
  world.assets.floorCameraUpLoc = GetShaderLocation(world.assets.floorShader, "cameraUp");
  world.assets.floorResolutionLoc = GetShaderLocation(world.assets.floorShader, "resolution");
  world.assets.floorVerticalFovLoc = GetShaderLocation(world.assets.floorShader, "verticalFovRadians");
  world.assets.floorAspectRatioLoc = GetShaderLocation(world.assets.floorShader, "aspectRatio");
  world.assets.floorTimeSecondsLoc = GetShaderLocation(world.assets.floorShader, "timeSeconds");
  world.assets.floorBlockGridSizeLoc = GetShaderLocation(world.assets.floorShader, "blockGridSize");
  world.assets.floorChunkGridSizeLoc = GetShaderLocation(world.assets.floorShader, "chunkGridSize");
  world.assets.voxelCameraPositionLoc = GetShaderLocation(world.assets.voxelShader, "cameraPosition");
  world.assets.voxelTimeSecondsLoc = GetShaderLocation(world.assets.voxelShader, "timeSeconds");
  world.assets.voxelBlockColorLoc = GetShaderLocation(world.assets.voxelShader, "blockColor");
  world.assets.voxelFaceOcclusionALoc = GetShaderLocation(world.assets.voxelShader, "faceOcclusionA");
  world.assets.voxelFaceOcclusionBLoc = GetShaderLocation(world.assets.voxelShader, "faceOcclusionB");
  world.assets.voxelModel.materials[0].shader = world.assets.voxelShader;

  if (IsSoundValid(world.assets.footstepSound)) {
    SetSoundVolume(world.assets.footstepSound, FOOTSTEP_VOLUME);
    SetSoundPitch(world.assets.footstepSound, FOOTSTEP_PITCH);
  }

  if (IsSoundValid(world.assets.landingSound)) {
    SetSoundVolume(world.assets.landingSound, LANDING_VOLUME);
    SetSoundPitch(world.assets.landingSound, LANDING_PITCH);
  }

  world.assets.initialized = true;
}

static int FloorDiv(int value, int divisor) {
  int quotient = value / divisor;
  int remainder = value % divisor;
  if (remainder != 0 && ((remainder < 0) != (divisor < 0))) quotient -= 1;
  return quotient;
}

static int PositiveModulo(int value, int divisor) {
  int result = value % divisor;
  return result < 0 ? result + divisor : result;
}

static VoxelChunk* FindChunk(int chunkX, int chunkY) {
  for (int i = 0; i < world.chunkCount; i++) {
    if (world.chunks[i].chunkX == chunkX && world.chunks[i].chunkY == chunkY) return &world.chunks[i];
  }

  return 0;
}

static VoxelChunk* GetOrCreateChunk(int chunkX, int chunkY) {
  VoxelChunk* chunk = FindChunk(chunkX, chunkY);
  if (chunk != 0) return chunk;
  if (world.chunkCount >= MAX_VOXEL_CHUNKS) return 0;

  chunk = &world.chunks[world.chunkCount++];
  memset(chunk, 0, sizeof(*chunk));
  chunk->chunkX = chunkX;
  chunk->chunkY = chunkY;
  return chunk;
}

static PlayerState* FindPlayer(int playerId) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (world.players[i].active && world.players[i].id == playerId) return &world.players[i];
  }

  return 0;
}

static PlayerState* SpawnPlayer(int playerId, Vector3 position, float yaw, float pitch) {
  PlayerState* existingPlayer = FindPlayer(playerId);
  if (existingPlayer != 0) {
    existingPlayer->position = position;
    existingPlayer->yaw = yaw;
    existingPlayer->pitch = pitch;
    existingPlayer->verticalVelocity = 0.0f;
    existingPlayer->jumpedSinceGrounded = false;
    existingPlayer->frameInput = (PlayerFrameInput) {0};
    return existingPlayer;
  }

  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (world.players[i].active) continue;

    world.players[i] = (PlayerState) {
        .id = playerId,
        .active = true,
        .position = position,
        .yaw = yaw,
        .pitch = pitch,
    };
    return &world.players[i];
  }

  return 0;
}

static void DespawnPlayer(int playerId) {
  PlayerState* player = FindPlayer(playerId);
  if (player == 0) return;
  *player = (PlayerState) {0};
}

static uint8_t GetVoxel(int voxelX, int voxelY, int voxelZ) {
  if (voxelZ < 0 || voxelZ >= CHUNK_MAX_HEIGHT) return 0;

  int chunkX = FloorDiv(voxelX, CHUNK_SIZE_XZ);
  int chunkY = FloorDiv(voxelY, CHUNK_SIZE_XZ);
  int localX = PositiveModulo(voxelX, CHUNK_SIZE_XZ);
  int localY = PositiveModulo(voxelY, CHUNK_SIZE_XZ);
  VoxelChunk* chunk = FindChunk(chunkX, chunkY);

  if (chunk == 0) return 0;
  return chunk->voxels[voxelZ][localY][localX];
}

static void SetVoxel(int voxelX, int voxelY, int voxelZ, uint8_t voxelValue) {
  if (voxelZ < 0 || voxelZ >= CHUNK_MAX_HEIGHT) return;

  int chunkX = FloorDiv(voxelX, CHUNK_SIZE_XZ);
  int chunkY = FloorDiv(voxelY, CHUNK_SIZE_XZ);
  int localX = PositiveModulo(voxelX, CHUNK_SIZE_XZ);
  int localY = PositiveModulo(voxelY, CHUNK_SIZE_XZ);
  VoxelChunk* chunk = GetOrCreateChunk(chunkX, chunkY);

  if (chunk == 0) return;
  chunk->voxels[voxelZ][localY][localX] = voxelValue;
}

static Color GetVoxelDisplayColor(uint8_t voxelValue) {
  if (voxelValue == 0) return BLANK;
  if (voxelValue == 255) return WHITE;

  float hue = ((float)(voxelValue - 1) / 254.0f) * 360.0f;
  return ColorFromHSV(hue, 0.85f, 1.0f);
}

static Vector3 GetVoxelCenter(int voxelX, int voxelY, int voxelZ) {
  return (Vector3) {
      ((float)voxelX + 0.5f) * BLOCK_GRID_SIZE,
      ((float)voxelY + 0.5f) * BLOCK_GRID_SIZE,
      ((float)voxelZ + 0.5f) * BLOCK_GRID_SIZE,
  };
}

static bool IsVoxelFilled(int voxelX, int voxelY, int voxelZ) {
  return GetVoxel(voxelX, voxelY, voxelZ) != 0;
}

static bool IsVoxelFullyEnclosed(int voxelX, int voxelY, int voxelZ) {
  return IsVoxelFilled(voxelX + 1, voxelY, voxelZ) && IsVoxelFilled(voxelX - 1, voxelY, voxelZ) &&
         IsVoxelFilled(voxelX, voxelY + 1, voxelZ) && IsVoxelFilled(voxelX, voxelY - 1, voxelZ) &&
         IsVoxelFilled(voxelX, voxelY, voxelZ + 1) && IsVoxelFilled(voxelX, voxelY, voxelZ - 1);
}

static float ComputeFaceAmbientOcclusion(int voxelX, int voxelY, int voxelZ, int normalX, int normalY, int normalZ) {
  int occupiedNeighbors = 0;
  int sampleCount = 0;

  if (normalX != 0) {
    for (int offsetY = -1; offsetY <= 1; offsetY++) {
      for (int offsetZ = -1; offsetZ <= 1; offsetZ++) {
        if (offsetY == 0 && offsetZ == 0) continue;
        if (IsVoxelFilled(voxelX + normalX, voxelY + offsetY, voxelZ + offsetZ)) occupiedNeighbors += 1;
        sampleCount += 1;
      }
    }
  } else if (normalY != 0) {
    for (int offsetX = -1; offsetX <= 1; offsetX++) {
      for (int offsetZ = -1; offsetZ <= 1; offsetZ++) {
        if (offsetX == 0 && offsetZ == 0) continue;
        if (IsVoxelFilled(voxelX + offsetX, voxelY + normalY, voxelZ + offsetZ)) occupiedNeighbors += 1;
        sampleCount += 1;
      }
    }
  } else {
    for (int offsetX = -1; offsetX <= 1; offsetX++) {
      for (int offsetY = -1; offsetY <= 1; offsetY++) {
        if (offsetX == 0 && offsetY == 0) continue;
        if (IsVoxelFilled(voxelX + offsetX, voxelY + offsetY, voxelZ + normalZ)) occupiedNeighbors += 1;
        sampleCount += 1;
      }
    }
  }

  if (sampleCount == 0) return 1.0f;

  float occlusion = (float)occupiedNeighbors / (float)sampleCount;
  return Clamp(1.0f - occlusion * 0.55f, 0.35f, 1.0f);
}

static void GetPlayerBounds(Vector3 playerPosition, Vector3* minBounds, Vector3* maxBounds) {
  *minBounds = (Vector3) {playerPosition.x - PLAYER_RADIUS, playerPosition.y - PLAYER_RADIUS, playerPosition.z};
  *maxBounds = (Vector3) {playerPosition.x + PLAYER_RADIUS, playerPosition.y + PLAYER_RADIUS, playerPosition.z + PLAYER_HEIGHT};
}

static bool IsAabbCollidingWithVoxels(Vector3 minBounds, Vector3 maxBounds) {
  const float epsilon = 0.001f;
  int minVoxelX = (int)floorf(minBounds.x / BLOCK_GRID_SIZE);
  int maxVoxelX = (int)floorf((maxBounds.x - epsilon) / BLOCK_GRID_SIZE);
  int minVoxelY = (int)floorf(minBounds.y / BLOCK_GRID_SIZE);
  int maxVoxelY = (int)floorf((maxBounds.y - epsilon) / BLOCK_GRID_SIZE);
  int minVoxelZ = (int)floorf(minBounds.z / BLOCK_GRID_SIZE);
  int maxVoxelZ = (int)floorf((maxBounds.z - epsilon) / BLOCK_GRID_SIZE);

  for (int voxelZ = minVoxelZ; voxelZ <= maxVoxelZ; voxelZ++) {
    for (int voxelY = minVoxelY; voxelY <= maxVoxelY; voxelY++) {
      for (int voxelX = minVoxelX; voxelX <= maxVoxelX; voxelX++) {
        if (GetVoxel(voxelX, voxelY, voxelZ) != 0) return true;
      }
    }
  }

  return false;
}

static bool IsPlayerColliding(Vector3 playerPosition) {
  Vector3 minBounds;
  Vector3 maxBounds;
  GetPlayerBounds(playerPosition, &minBounds, &maxBounds);
  return IsAabbCollidingWithVoxels(minBounds, maxBounds);
}

static CollisionMoveResult MovePlayerAlongAxis(Vector3* playerPosition, int axisIndex, float delta) {
  CollisionMoveResult result = {0};
  if (delta == 0.0f) return result;

  float maxStep = BLOCK_GRID_SIZE * 0.2f;
  int steps = (int)ceilf(fabsf(delta) / maxStep);
  if (steps < 1) steps = 1;

  float stepDelta = delta / (float)steps;
  for (int i = 0; i < steps; i++) {
    Vector3 nextPosition = *playerPosition;
    if (axisIndex == 0) nextPosition.x += stepDelta;
    if (axisIndex == 1) nextPosition.y += stepDelta;
    if (axisIndex == 2) nextPosition.z += stepDelta;
    if (nextPosition.z < GROUND_HEIGHT) nextPosition.z = GROUND_HEIGHT;

    if (IsPlayerColliding(nextPosition)) {
      result.collided = true;
      break;
    }

    *playerPosition = nextPosition;
    result.movedDelta += stepDelta;
  }

  return result;
}

static bool IsPlayerGrounded(Vector3 playerPosition) {
  const float groundProbeDistance = 0.05f;
  if (playerPosition.z <= GROUND_HEIGHT + groundProbeDistance) return true;

  Vector3 probePosition = playerPosition;
  probePosition.z -= groundProbeDistance;
  if (probePosition.z < GROUND_HEIGHT) probePosition.z = GROUND_HEIGHT;
  return IsPlayerColliding(probePosition);
}

static void ResolvePlayerPenetration(Vector3* playerPosition) {
  if (!IsPlayerColliding(*playerPosition)) return;

  const float stepSize = BLOCK_GRID_SIZE * 0.1f;
  const int maxSteps = 80;
  const Vector3 directions[] = {
      {        0.0f,         0.0f,        1.0f},
      {        1.0f,         0.0f,        0.0f},
      {       -1.0f,         0.0f,        0.0f},
      {        0.0f,         1.0f,        0.0f},
      {        0.0f,        -1.0f,        0.0f},
      { 0.70710677f,  0.70710677f,        0.0f},
      { 0.70710677f, -0.70710677f,        0.0f},
      {-0.70710677f,  0.70710677f,        0.0f},
      {-0.70710677f, -0.70710677f,        0.0f},
      { 0.70710677f,         0.0f, 0.70710677f},
      {-0.70710677f,         0.0f, 0.70710677f},
      {        0.0f,  0.70710677f, 0.70710677f},
      {        0.0f, -0.70710677f, 0.70710677f},
  };

  Vector3 startPosition = *playerPosition;
  for (int stepIndex = 1; stepIndex <= maxSteps; stepIndex++) {
    float distance = stepSize * (float)stepIndex;
    for (int directionIndex = 0; directionIndex < (int)(sizeof(directions) / sizeof(directions[0])); directionIndex++) {
      Vector3 candidate = Vector3Add(startPosition, Vector3Scale(directions[directionIndex], distance));
      if (candidate.z < GROUND_HEIGHT) candidate.z = GROUND_HEIGHT;
      if (!IsPlayerColliding(candidate)) {
        *playerPosition = candidate;
        return;
      }
    }
  }
}

static VoxelRaycastHit RaycastVoxelWorld(Vector3 origin, Vector3 direction, float maxDistance) {
  VoxelRaycastHit result = {0};
  Vector3 rayDirection = Vector3Normalize(direction);
  int voxelX = (int)floorf(origin.x / BLOCK_GRID_SIZE);
  int voxelY = (int)floorf(origin.y / BLOCK_GRID_SIZE);
  int voxelZ = (int)floorf(origin.z / BLOCK_GRID_SIZE);
  int previousVoxelX = voxelX;
  int previousVoxelY = voxelY;
  int previousVoxelZ = voxelZ;

  int stepX = rayDirection.x > 0.0f ? 1 : (rayDirection.x < 0.0f ? -1 : 0);
  int stepY = rayDirection.y > 0.0f ? 1 : (rayDirection.y < 0.0f ? -1 : 0);
  int stepZ = rayDirection.z > 0.0f ? 1 : (rayDirection.z < 0.0f ? -1 : 0);

  float nextBoundaryX = (stepX > 0 ? (float)(voxelX + 1) : (float)voxelX) * BLOCK_GRID_SIZE;
  float nextBoundaryY = (stepY > 0 ? (float)(voxelY + 1) : (float)voxelY) * BLOCK_GRID_SIZE;
  float nextBoundaryZ = (stepZ > 0 ? (float)(voxelZ + 1) : (float)voxelZ) * BLOCK_GRID_SIZE;

  float tMaxX = stepX == 0 ? INFINITY : (nextBoundaryX - origin.x) / rayDirection.x;
  float tMaxY = stepY == 0 ? INFINITY : (nextBoundaryY - origin.y) / rayDirection.y;
  float tMaxZ = stepZ == 0 ? INFINITY : (nextBoundaryZ - origin.z) / rayDirection.z;
  float tDeltaX = stepX == 0 ? INFINITY : BLOCK_GRID_SIZE / fabsf(rayDirection.x);
  float tDeltaY = stepY == 0 ? INFINITY : BLOCK_GRID_SIZE / fabsf(rayDirection.y);
  float tDeltaZ = stepZ == 0 ? INFINITY : BLOCK_GRID_SIZE / fabsf(rayDirection.z);
  float travelDistance = 0.0f;

  if (GetVoxel(voxelX, voxelY, voxelZ) != 0) {
    result.hit = true;
    result.voxelX = voxelX;
    result.voxelY = voxelY;
    result.voxelZ = voxelZ;
    result.placeX = voxelX;
    result.placeY = voxelY;
    result.placeZ = voxelZ;
    return result;
  }

  for (int i = 0; i < 512 && travelDistance <= maxDistance; i++) {
    previousVoxelX = voxelX;
    previousVoxelY = voxelY;
    previousVoxelZ = voxelZ;

    if (tMaxX < tMaxY && tMaxX < tMaxZ) {
      voxelX += stepX;
      travelDistance = tMaxX;
      tMaxX += tDeltaX;
    } else if (tMaxY < tMaxZ) {
      voxelY += stepY;
      travelDistance = tMaxY;
      tMaxY += tDeltaY;
    } else {
      voxelZ += stepZ;
      travelDistance = tMaxZ;
      tMaxZ += tDeltaZ;
    }

    if (travelDistance > maxDistance) break;
    if (GetVoxel(voxelX, voxelY, voxelZ) == 0) continue;

    result.hit = true;
    result.voxelX = voxelX;
    result.voxelY = voxelY;
    result.voxelZ = voxelZ;
    result.placeX = previousVoxelX;
    result.placeY = previousVoxelY;
    result.placeZ = previousVoxelZ;
    return result;
  }

  if (rayDirection.z < 0.0f) {
    float groundDistance = -origin.z / rayDirection.z;
    if (groundDistance > 0.0f && groundDistance <= maxDistance) {
      Vector3 groundHit = Vector3Add(origin, Vector3Scale(rayDirection, groundDistance));
      result.placeOnGround = true;
      result.placeX = (int)floorf(groundHit.x / BLOCK_GRID_SIZE);
      result.placeY = (int)floorf(groundHit.y / BLOCK_GRID_SIZE);
      result.placeZ = 0;
    }
  }

  return result;
}

static void SeedVoxelWorld(void) {
}

static void ResetFrameInputs(void) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (!world.players[i].active) continue;
    world.players[i].frameInput = (PlayerFrameInput) {0};
  }
}

static void ProcessEventQueue(void) {
  ResetFrameInputs();

  for (int i = 0; i < world.eventCount; i++) {
    const WorldEvent* event = &world.eventQueue[i];

    switch (event->type) {
      case WORLD_EVENT_TYPE_SPAWN:
        SpawnPlayer(event->playerId, (Vector3) {event->playerX, event->playerY, event->playerZ}, event->playerYaw, event->playerPitch);
        break;
      case WORLD_EVENT_TYPE_DESPAWN:
        DespawnPlayer(event->playerId);
        break;
      case WORLD_EVENT_TYPE_MOVE:
      case WORLD_EVENT_TYPE_LOOK:
      case WORLD_EVENT_TYPE_JUMP:
      case WORLD_EVENT_TYPE_PLACE: {
        PlayerState* player = FindPlayer(event->playerId);
        if (player == 0) break;

        if (event->type == WORLD_EVENT_TYPE_MOVE) {
          player->frameInput.moveX = event->moveX;
          player->frameInput.moveY = event->moveY;
        } else if (event->type == WORLD_EVENT_TYPE_LOOK) {
          player->frameInput.lookDeltaX += event->lookDeltaX;
          player->frameInput.lookDeltaY += event->lookDeltaY;
        } else if (event->type == WORLD_EVENT_TYPE_JUMP) {
          player->frameInput.jumpRequested = true;
        } else {
          player->frameInput.placeRequested = true;
          player->frameInput.placeVoxelValue = event->voxelValue;
        }
      } break;
    }
  }

  world.eventCount = 0;
}

static void UpdatePlayerLook(PlayerState* player) {
  player->yaw -= player->frameInput.lookDeltaX * MOUSE_SENSITIVITY;
  player->pitch -= player->frameInput.lookDeltaY * MOUSE_SENSITIVITY;
  player->pitch = Clamp(player->pitch, -MAX_ABS_PITCH, MAX_ABS_PITCH);
}

static void UpdatePlayerMovement(PlayerState* player) {
  float frameTime = GetFrameTime();
  bool isGrounded = IsPlayerGrounded(player->position);
  bool wasGrounded = isGrounded;

  if (isGrounded && player->verticalVelocity < 0.0f) player->verticalVelocity = 0.0f;

  if (isGrounded && player->frameInput.jumpRequested) {
    player->verticalVelocity = JUMP_VELOCITY;
    player->jumpedSinceGrounded = true;
  }

  Vector3 movementForward = {cosf(player->yaw), sinf(player->yaw), 0.0f};
  Vector3 movementRight = {movementForward.y, -movementForward.x, 0.0f};
  Vector3 movement = {0.0f, 0.0f, 0.0f};

  movement = Vector3Add(movement, Vector3Scale(movementForward, player->frameInput.moveY));
  movement = Vector3Add(movement, Vector3Scale(movementRight, player->frameInput.moveX));

  bool isWalking = Vector3LengthSqr(movement) > 0.0f;
  if (isWalking) {
    movement = Vector3Scale(Vector3Normalize(movement), MOVE_SPEED * frameTime);
    MovePlayerAlongAxis(&player->position, 0, movement.x);
    MovePlayerAlongAxis(&player->position, 1, movement.y);

    if (player->id == 0 && isGrounded && IsSoundValid(world.assets.footstepSound)) {
      player->footstepTimer -= frameTime;
      if (player->footstepTimer <= 0.0f) {
        PlaySound(world.assets.footstepSound);
        player->footstepTimer = FOOTSTEP_INTERVAL_SECONDS;
      }
    }
  } else {
    player->footstepTimer = 0.0f;
  }

  player->verticalVelocity -= GRAVITY * frameTime;
  CollisionMoveResult verticalMove = MovePlayerAlongAxis(&player->position, 2, player->verticalVelocity * frameTime);
  if (verticalMove.collided) player->verticalVelocity = 0.0f;

  isGrounded = IsPlayerGrounded(player->position);
  if (isGrounded && player->verticalVelocity < 0.0f) player->verticalVelocity = 0.0f;
  if (player->id == 0 && !wasGrounded && isGrounded && player->jumpedSinceGrounded) {
    if (IsSoundValid(world.assets.landingSound)) PlaySound(world.assets.landingSound);
    player->jumpedSinceGrounded = false;
    player->footstepTimer = FOOTSTEP_INTERVAL_SECONDS;
  }

  ResolvePlayerPenetration(&player->position);
}

static void GetPlayerCameraVectors(const PlayerState* player, Vector3* cameraPosition, Vector3* cameraForward, Vector3* cameraRight, Vector3* cameraUp) {
  *cameraPosition = (Vector3) {player->position.x, player->position.y, player->position.z + EYE_HEIGHT};
  *cameraForward = (Vector3) {
      cosf(player->pitch) * cosf(player->yaw),
      cosf(player->pitch) * sinf(player->yaw),
      sinf(player->pitch),
  };
  *cameraRight = Vector3Normalize(Vector3CrossProduct(*cameraForward, WORLD_UP));
  *cameraUp = Vector3Normalize(Vector3CrossProduct(*cameraRight, *cameraForward));
}

static void HandlePlayerVoxelAction(PlayerState* player) {
  if (!player->frameInput.placeRequested) return;

  Vector3 cameraPosition;
  Vector3 cameraForward;
  Vector3 cameraRight;
  Vector3 cameraUp;
  GetPlayerCameraVectors(player, &cameraPosition, &cameraForward, &cameraRight, &cameraUp);

  VoxelRaycastHit voxelHit = RaycastVoxelWorld(cameraPosition, cameraForward, VOXEL_EDIT_DISTANCE);

  if (player->frameInput.placeVoxelValue == 0) {
    if (voxelHit.hit) SetVoxel(voxelHit.voxelX, voxelHit.voxelY, voxelHit.voxelZ, 0);
    return;
  }

  bool canPlaceVoxel = voxelHit.hit || voxelHit.placeOnGround;
  if (!canPlaceVoxel) return;
  if (GetVoxel(voxelHit.placeX, voxelHit.placeY, voxelHit.placeZ) != 0) return;

  SetVoxel(voxelHit.placeX, voxelHit.placeY, voxelHit.placeZ, player->frameInput.placeVoxelValue);
  if (IsPlayerColliding(player->position)) {
    SetVoxel(voxelHit.placeX, voxelHit.placeY, voxelHit.placeZ, 0);
  }
}

static void UpdateShaders(Vector3 cameraPosition, Vector3 cameraForward, Vector3 cameraRight, Vector3 cameraUp) {
  Vector2 resolution = {(float)GetScreenWidth(), (float)GetScreenHeight()};
  float aspectRatio = resolution.x / resolution.y;
  float timeSeconds = (float)GetTime();
  float chunkGridSize = BLOCK_GRID_SIZE * (float)CHUNK_SIZE_XZ;
  float verticalFovRadians = VERTICAL_FOV_RADIANS;
  float blockGridSize = BLOCK_GRID_SIZE;

  SetShaderValue(world.assets.floorShader, world.assets.floorCameraPositionLoc, &cameraPosition, SHADER_UNIFORM_VEC3);
  SetShaderValue(world.assets.floorShader, world.assets.floorCameraForwardLoc, &cameraForward, SHADER_UNIFORM_VEC3);
  SetShaderValue(world.assets.floorShader, world.assets.floorCameraRightLoc, &cameraRight, SHADER_UNIFORM_VEC3);
  SetShaderValue(world.assets.floorShader, world.assets.floorCameraUpLoc, &cameraUp, SHADER_UNIFORM_VEC3);
  SetShaderValue(world.assets.floorShader, world.assets.floorResolutionLoc, &resolution, SHADER_UNIFORM_VEC2);
  SetShaderValue(world.assets.floorShader, world.assets.floorVerticalFovLoc, &verticalFovRadians, SHADER_UNIFORM_FLOAT);
  SetShaderValue(world.assets.floorShader, world.assets.floorAspectRatioLoc, &aspectRatio, SHADER_UNIFORM_FLOAT);
  SetShaderValue(world.assets.floorShader, world.assets.floorTimeSecondsLoc, &timeSeconds, SHADER_UNIFORM_FLOAT);
  SetShaderValue(world.assets.floorShader, world.assets.floorBlockGridSizeLoc, &blockGridSize, SHADER_UNIFORM_FLOAT);
  SetShaderValue(world.assets.floorShader, world.assets.floorChunkGridSizeLoc, &chunkGridSize, SHADER_UNIFORM_FLOAT);
  SetShaderValue(world.assets.voxelShader, world.assets.voxelCameraPositionLoc, &cameraPosition, SHADER_UNIFORM_VEC3);
  SetShaderValue(world.assets.voxelShader, world.assets.voxelTimeSecondsLoc, &timeSeconds, SHADER_UNIFORM_FLOAT);
}

static void DrawVoxelWorld(void) {
  Vector3 voxelScale = {BLOCK_GRID_SIZE, BLOCK_GRID_SIZE, BLOCK_GRID_SIZE};

  for (int chunkIndex = 0; chunkIndex < world.chunkCount; chunkIndex++) {
    for (int z = 0; z < CHUNK_MAX_HEIGHT; z++) {
      for (int localY = 0; localY < CHUNK_SIZE_XZ; localY++) {
        for (int localX = 0; localX < CHUNK_SIZE_XZ; localX++) {
          uint8_t voxelValue = world.chunks[chunkIndex].voxels[z][localY][localX];
          if (voxelValue == 0) continue;

          int voxelX = world.chunks[chunkIndex].chunkX * CHUNK_SIZE_XZ + localX;
          int voxelY = world.chunks[chunkIndex].chunkY * CHUNK_SIZE_XZ + localY;
          if (IsVoxelFullyEnclosed(voxelX, voxelY, z)) continue;

          Vector3 voxelCenter = GetVoxelCenter(voxelX, voxelY, z);
          Color voxelColor = GetVoxelDisplayColor(voxelValue);
          float voxelColorNormalized[3] = {
              (float)voxelColor.r / 255.0f,
              (float)voxelColor.g / 255.0f,
              (float)voxelColor.b / 255.0f,
          };
          float voxelFaceOcclusionA[3] = {
              ComputeFaceAmbientOcclusion(voxelX, voxelY, z, 1, 0, 0),
              ComputeFaceAmbientOcclusion(voxelX, voxelY, z, -1, 0, 0),
              ComputeFaceAmbientOcclusion(voxelX, voxelY, z, 0, 1, 0),
          };
          float voxelFaceOcclusionB[3] = {
              ComputeFaceAmbientOcclusion(voxelX, voxelY, z, 0, -1, 0),
              ComputeFaceAmbientOcclusion(voxelX, voxelY, z, 0, 0, 1),
              ComputeFaceAmbientOcclusion(voxelX, voxelY, z, 0, 0, -1),
          };

          SetShaderValue(world.assets.voxelShader, world.assets.voxelBlockColorLoc, voxelColorNormalized, SHADER_UNIFORM_VEC3);
          SetShaderValue(world.assets.voxelShader, world.assets.voxelFaceOcclusionALoc, voxelFaceOcclusionA, SHADER_UNIFORM_VEC3);
          SetShaderValue(world.assets.voxelShader, world.assets.voxelFaceOcclusionBLoc, voxelFaceOcclusionB, SHADER_UNIFORM_VEC3);
          DrawModelEx(world.assets.voxelModel, voxelCenter, WORLD_UP, 0.0f, voxelScale, WHITE);
        }
      }
    }
  }
}

static void DrawOtherPlayers(int cameraPlayerId) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    const PlayerState* player = &world.players[i];
    if (!player->active || player->id == cameraPlayerId) continue;

    Vector3 center = player->position;
    center.z += PLAYER_HEIGHT * 0.5f;
    DrawCubeV(center, (Vector3) {PLAYER_RADIUS * 2.0f, PLAYER_RADIUS * 2.0f, PLAYER_HEIGHT}, (Color) {120, 220, 255, 180});
    DrawCubeWiresV(center, (Vector3) {PLAYER_RADIUS * 2.0f, PLAYER_RADIUS * 2.0f, PLAYER_HEIGHT}, RAYWHITE);
  }
}

void InitWorld(void) {
  memset(&world, 0, sizeof(world));
  world.initialized = true;

  SeedVoxelWorld();
}

void QuitWorld(void) {
  if (!world.initialized) return;

  if (world.assets.initialized) {
    if (IsSoundValid(world.assets.footstepSound)) UnloadSound(world.assets.footstepSound);
    if (IsSoundValid(world.assets.landingSound)) UnloadSound(world.assets.landingSound);
    UnloadModel(world.assets.voxelModel);
    UnloadShader(world.assets.voxelShader);
    UnloadShader(world.assets.floorShader);
  }

  memset(&world, 0, sizeof(world));
}

void SendWorldEvent(const WorldEvent* event) {
  if (event == 0 || !world.initialized) return;
  if (world.eventCount >= MAX_WORLD_EVENTS) return;

  world.eventQueue[world.eventCount++] = *event;
}

void UpdateWorld(void) {
  if (!world.initialized) return;

  ProcessEventQueue();

  for (int i = 0; i < MAX_PLAYERS; i++) {
    PlayerState* player = &world.players[i];
    if (!player->active) continue;
    UpdatePlayerLook(player);
    UpdatePlayerMovement(player);
  }

  for (int i = 0; i < MAX_PLAYERS; i++) {
    PlayerState* player = &world.players[i];
    if (!player->active) continue;
    HandlePlayerVoxelAction(player);
  }
}

void RenderWorld(int playerId) {
  if (!world.initialized) return;

  EnsureRenderAssetsInitialized();

  PlayerState* player = FindPlayer(playerId);
  if (player == 0) return;

  Vector3 cameraPosition;
  Vector3 cameraForward;
  Vector3 cameraRight;
  Vector3 cameraUp;
  GetPlayerCameraVectors(player, &cameraPosition, &cameraForward, &cameraRight, &cameraUp);
  UpdateShaders(cameraPosition, cameraForward, cameraRight, cameraUp);

  ClearBackground(BLACK);
  BeginShaderMode(world.assets.floorShader);
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), WHITE);
  EndShaderMode();

  Camera3D camera = {0};
  camera.position = cameraPosition;
  camera.target = Vector3Add(cameraPosition, cameraForward);
  camera.up = WORLD_UP;
  camera.fovy = VERTICAL_FOV_DEGREES;
  camera.projection = CAMERA_PERSPECTIVE;

  BeginMode3D(camera);
  DrawVoxelWorld();
  DrawOtherPlayers(playerId);
  EndMode3D();
}
