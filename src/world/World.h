#pragma once

#include "PlayerSystem.h"
#include "VoxelWorld.h"
#include "WorldAssets.h"
#include "WorldEvent.h"
#include "WorldEventQueue.h"
#include "WorldMeshSystem.h"
#include "WorldRenderer.h"

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
  void Update();
  void Render(int playerId);

 private:
  bool initialized_ = false;
  VoxelWorld voxelWorld;
  PlayerSystem playerSystem;
  WorldAssets assets;
  WorldEventQueue eventQueue;
  WorldMeshSystem meshSystem;
  WorldRenderer renderer;
};
