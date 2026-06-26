#pragma once

#include "../TaskManager.h"
#include "../Utils.h"
#include "../world/World.h"
#include "AssetManager.h"
#include "SoundPlayer.h"
#include "WorldClientData.h"
#include "WorldMeshSystem.h"

class ClientWorld : public NonCopyable
{
 public:
  ClientWorld() = default;
  ~ClientWorld();

  void Initialize(TaskManager& taskManager);
  void Shutdown();
  void Update(World& world);
  void Render(const World& world, int playerId);

 private:
  void PlayLocalPlayerSounds(const PlayerSystem& playerSystem);

  bool initialized = false;
  AssetManager assetManager;
  SoundPlayer soundPlayer;
  WorldMeshSystem meshSystem;
  WorldClientData clientData;
};
