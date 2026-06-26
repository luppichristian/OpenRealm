#pragma once

#include "../AssetManager.h"
#include "../SoundPlayer.h"
#include "PlayerSystem.h"
#include "VoxelWorld.h"
#include "WorldEvent.h"
#include "WorldEventQueue.h"
#include "WorldMeshSystem.h"

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
  bool initialized = false;
  VoxelWorld voxelWorld;
  PlayerSystem playerSystem;
  AssetManager assetManager;
  SoundPlayer soundPlayer;
  WorldEventQueue eventQueue;
  WorldMeshSystem meshSystem;
};
