#pragma once

#include "../world/World.h"
#include "AssetManager.h"
#include "SoundPlayer.h"
#include "WorldClientData.h"
#include "WorldMeshSystem.h"

class ClientWorld
{
 public:
  ClientWorld() = default;
  ~ClientWorld();

  ClientWorld(const ClientWorld&) = delete;
  ClientWorld& operator=(const ClientWorld&) = delete;

  void Initialize();
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
