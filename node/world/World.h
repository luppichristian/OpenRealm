#pragma once

#include <queue>
#include "../Utils.h"
#include "PlayerSystem.h"
#include "VoxelWorld.h"
#include "WorldEvent.h"

class World : public NonCopyable
{
 public:
  World() = default;
  ~World();

  void Initialize();
  void Shutdown();
  void SendEvent(const WorldEvent& event);
  void Update(float frameTime);

  VoxelWorld& GetVoxelWorld();
  const VoxelWorld& GetVoxelWorld() const;
  PlayerSystem& GetPlayerSystem();
  const PlayerSystem& GetPlayerSystem() const;

 private:
  bool initialized = false;
  VoxelWorld voxelWorld;
  PlayerSystem playerSystem;
  std::queue<WorldEvent> eventQueue;
};
