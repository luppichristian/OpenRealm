#pragma once

#include <array>

#include "VoxelWorld.h"
#include "WorldEvent.h"

class PlayerSystem
{
 public:
  void Initialize();
  void ResetFrameInputs();
  void ProcessEvent(const WorldEvent& event);
  void Update(float frameTime, VoxelWorld& voxelWorld);

  PlayerState* FindPlayer(int playerId);
  const PlayerState* FindPlayer(int playerId) const;
  const std::array<PlayerState, MAX_PLAYERS>& GetPlayers() const;
  void GetCameraVectors(const PlayerState* player, Vector3* cameraPosition, Vector3* cameraForward, Vector3* cameraRight, Vector3* cameraUp) const;

 private:
  PlayerState* SpawnPlayer(int playerId, Vector3 position, float yaw, float pitch);
  void DespawnPlayer(int playerId);
  void UpdatePlayerLook(PlayerState* player) const;
  void UpdatePlayerMovement(PlayerState* player, float frameTime, VoxelWorld& voxelWorld) const;
  void HandlePlayerVoxelAction(PlayerState* player, VoxelWorld& voxelWorld) const;

  std::array<PlayerState, MAX_PLAYERS> players = {};
};
