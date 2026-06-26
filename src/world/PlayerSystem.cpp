#include "PlayerSystem.h"

void PlayerSystem::Initialize()
{
  players.fill({});
}

PlayerState* PlayerSystem::FindPlayer(int playerId)
{
  for (PlayerState& player : players)
  {
    if (player.active && player.id == playerId) return &player;
  }

  return nullptr;
}

const PlayerState* PlayerSystem::FindPlayer(int playerId) const
{
  for (const PlayerState& player : players)
  {
    if (player.active && player.id == playerId) return &player;
  }

  return nullptr;
}

PlayerState* PlayerSystem::SpawnPlayer(int playerId, Vector3 position, float yaw, float pitch)
{
  PlayerState* existingPlayer = FindPlayer(playerId);
  if (existingPlayer != nullptr)
  {
    existingPlayer->position = position;
    existingPlayer->yaw = yaw;
    existingPlayer->pitch = pitch;
    existingPlayer->verticalVelocity = 0.0f;
    existingPlayer->jumpedSinceGrounded = false;
    existingPlayer->isGrounded = true;
    existingPlayer->frameInput = {};
    return existingPlayer;
  }

  for (PlayerState& player : players)
  {
    if (player.active) continue;

    player = {};
    player.id = playerId;
    player.active = true;
    player.isGrounded = true;
    player.position = position;
    player.yaw = yaw;
    player.pitch = pitch;
    return &player;
  }

  return nullptr;
}

void PlayerSystem::DespawnPlayer(int playerId)
{
  PlayerState* player = FindPlayer(playerId);
  if (player != nullptr) *player = {};
}

void PlayerSystem::ResetFrameInputs()
{
  for (PlayerState& player : players)
  {
    if (!player.active) continue;
    player.frameInput = {};
  }
}

void PlayerSystem::ProcessEvent(const WorldEvent& event)
{
  switch (event.type)
  {
    case WorldEventType::Spawn:
      SpawnPlayer(event.playerId, {event.playerX, event.playerY, event.playerZ}, event.playerYaw, event.playerPitch);
      break;
    case WorldEventType::Despawn:
      DespawnPlayer(event.playerId);
      break;
    case WorldEventType::Move:
    case WorldEventType::Look:
    case WorldEventType::Jump:
    case WorldEventType::Place:
    {
      PlayerState* player = FindPlayer(event.playerId);
      if (player == nullptr) break;

      if (event.type == WorldEventType::Move)
      {
        player->frameInput.moveX = event.moveX;
        player->frameInput.moveY = event.moveY;
      }
      else if (event.type == WorldEventType::Look)
      {
        player->frameInput.lookDeltaX += event.lookDeltaX;
        player->frameInput.lookDeltaY += event.lookDeltaY;
      }
      else if (event.type == WorldEventType::Jump)
      {
        player->frameInput.jumpRequested = true;
      }
      else
      {
        player->frameInput.placeRequested = true;
        player->frameInput.placeVoxelValue = event.voxelValue;
      }
    }
    break;
  }
}

void PlayerSystem::UpdatePlayerLook(PlayerState* player) const
{
  player->yaw -= player->frameInput.lookDeltaX * MOUSE_SENSITIVITY;
  player->pitch -= player->frameInput.lookDeltaY * MOUSE_SENSITIVITY;
  player->pitch = Clamp(player->pitch, -MAX_ABS_PITCH, MAX_ABS_PITCH);
}

void PlayerSystem::UpdatePlayerMovement(PlayerState* player, float frameTime, VoxelWorld& voxelWorld, SoundPlayer& soundPlayer) const
{
  bool wasGrounded = player->isGrounded;
  bool wasWalkingOnGround = wasGrounded && (fabsf(player->frameInput.moveX) > 0.0f || fabsf(player->frameInput.moveY) > 0.0f);

  if (player->isGrounded && player->verticalVelocity < 0.0f) player->verticalVelocity = 0.0f;

  if (player->isGrounded && player->frameInput.jumpRequested)
  {
    player->verticalVelocity = JUMP_VELOCITY;
    player->isGrounded = false;
    player->jumpedSinceGrounded = true;
  }

  Vector3 movementForward = {cosf(player->yaw), sinf(player->yaw), 0.0f};
  Vector3 movementRight = {movementForward.y, -movementForward.x, 0.0f};
  Vector3 movement = Vector3Add(Vector3Scale(movementForward, player->frameInput.moveY), Vector3Scale(movementRight, player->frameInput.moveX));

  bool isWalking = Vector3LengthSqr(movement) > 0.0f;
  if (isWalking)
  {
    movement = Vector3Scale(Vector3Normalize(movement), MOVE_SPEED * frameTime);
    voxelWorld.MovePlayerAlongAxis(&player->position, 0, movement.x);
    voxelWorld.MovePlayerAlongAxis(&player->position, 1, movement.y);
  }

  player->verticalVelocity -= GRAVITY * frameTime;
  CollisionMoveResult verticalMove = voxelWorld.MovePlayerAlongAxis(&player->position, 2, player->verticalVelocity * frameTime);
  if (verticalMove.collided) player->verticalVelocity = 0.0f;
  player->isGrounded = verticalMove.hitFloor;

  if (player->id == 0 && isWalking && wasWalkingOnGround)
  {
    player->footstepTimer -= frameTime;
    if (player->footstepTimer <= 0.0f)
    {
      soundPlayer.PlayFootstep();
      player->footstepTimer = FOOTSTEP_INTERVAL_SECONDS;
    }
  }
  else if (!isWalking || !player->isGrounded)
  {
    player->footstepTimer = 0.0f;
  }

  if (player->id == 0 && !wasGrounded && player->isGrounded && player->jumpedSinceGrounded)
  {
    soundPlayer.PlayLanding();
    player->jumpedSinceGrounded = false;
    player->footstepTimer = FOOTSTEP_INTERVAL_SECONDS;
  }

  voxelWorld.ResolvePlayerPenetration(&player->position);
  if (!voxelWorld.IsPlayerColliding(player->position) && player->position.z <= GROUND_HEIGHT + 0.001f) player->isGrounded = true;
}

void PlayerSystem::GetCameraVectors(const PlayerState* player, Vector3* cameraPosition, Vector3* cameraForward, Vector3* cameraRight, Vector3* cameraUp) const
{
  *cameraPosition = {player->position.x, player->position.y, player->position.z + EYE_HEIGHT};
  *cameraForward = {cosf(player->pitch) * cosf(player->yaw), cosf(player->pitch) * sinf(player->yaw), sinf(player->pitch)};
  *cameraRight = Vector3Normalize(Vector3CrossProduct(*cameraForward, WORLD_UP));
  *cameraUp = Vector3Normalize(Vector3CrossProduct(*cameraRight, *cameraForward));
}

void PlayerSystem::HandlePlayerVoxelAction(PlayerState* player, VoxelWorld& voxelWorld) const
{
  if (!player->frameInput.placeRequested) return;

  Vector3 cameraPosition;
  Vector3 cameraForward;
  Vector3 cameraRight;
  Vector3 cameraUp;
  GetCameraVectors(player, &cameraPosition, &cameraForward, &cameraRight, &cameraUp);

  VoxelRaycastHit voxelHit = voxelWorld.Raycast(cameraPosition, cameraForward, VOXEL_EDIT_DISTANCE);

  if (player->frameInput.placeVoxelValue == 0)
  {
    if (voxelHit.hit) voxelWorld.SetVoxel(voxelHit.voxelX, voxelHit.voxelY, voxelHit.voxelZ, 0);
    return;
  }

  bool canPlaceVoxel = voxelHit.hit || voxelHit.placeOnGround;
  if (!canPlaceVoxel) return;
  if (voxelWorld.GetVoxel(voxelHit.placeX, voxelHit.placeY, voxelHit.placeZ) != 0) return;

  voxelWorld.SetVoxel(voxelHit.placeX, voxelHit.placeY, voxelHit.placeZ, player->frameInput.placeVoxelValue);
  if (voxelWorld.IsPlayerColliding(player->position))
  {
    voxelWorld.SetVoxel(voxelHit.placeX, voxelHit.placeY, voxelHit.placeZ, 0);
  }
}

void PlayerSystem::Update(float frameTime, VoxelWorld& voxelWorld, SoundPlayer& soundPlayer)
{
  for (PlayerState& player : players)
  {
    if (!player.active) continue;
    UpdatePlayerLook(&player);
    UpdatePlayerMovement(&player, frameTime, voxelWorld, soundPlayer);
    HandlePlayerVoxelAction(&player, voxelWorld);
  }
}

void PlayerSystem::DrawOtherPlayers(int cameraPlayerId) const
{
  for (const PlayerState& player : players)
  {
    if (!player.active || player.id == cameraPlayerId) continue;

    Vector3 center = player.position;
    center.z += PLAYER_HEIGHT * 0.5f;
    DrawCubeV(center, Vector3 {PLAYER_RADIUS * 2.0f, PLAYER_RADIUS * 2.0f, PLAYER_HEIGHT}, Color {120, 220, 255, 180});
    DrawCubeWiresV(center, {PLAYER_RADIUS * 2.0f, PLAYER_RADIUS * 2.0f, PLAYER_HEIGHT}, RAYWHITE);
  }
}
