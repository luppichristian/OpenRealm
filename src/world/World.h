#pragma once

#include "PlayerSystem.h"
#include "VoxelWorld.h"
#include "WorldEvent.h"
#include "WorldEventQueue.h"

class World
{
 public:
  World() = default;
  ~World();

  World(const World&) = delete;
  World& operator=(const World&) = delete;

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
  WorldEventQueue eventQueue;
};
