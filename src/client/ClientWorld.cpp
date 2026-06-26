#include "ClientWorld.h"

#include "PlayerController.h"
#include "WorldRenderer.h"

ClientWorld::~ClientWorld()
{
  Shutdown();
}

void ClientWorld::Initialize()
{
  if (initialized) return;

  soundPlayer.Initialize(assetManager);

  if (!meshSystem.Start())
  {
    TraceLog(LOG_WARNING, "Failed to start mesh worker threads; voxel mesh updates will be disabled.");
  }

  initialized = true;
}

void ClientWorld::Shutdown()
{
  if (!initialized) return;

  meshSystem.Stop(clientData);
  soundPlayer.Shutdown();
  assetManager.Shutdown();
  clientData = {};
  initialized = false;
}

void ClientWorld::PlayLocalPlayerSounds(const PlayerSystem& playerSystem)
{
  const PlayerState* localPlayer = playerSystem.FindPlayer(LOCAL_PLAYER_ID);
  if (localPlayer == nullptr) return;

  if (localPlayer->playFootstepSound) soundPlayer.PlayFootstep();
  if (localPlayer->playLandingSound) soundPlayer.PlayLanding();
}

void ClientWorld::Update(World& world)
{
  if (!initialized) return;

  PlayerSystem& playerSystem = world.GetPlayerSystem();
  VoxelWorld& voxelWorld = world.GetVoxelWorld();
  PlayLocalPlayerSounds(playerSystem);
  meshSystem.QueueDirtyChunkSectionMeshes(voxelWorld, clientData, CHUNK_MESH_QUEUE_BUDGET);
  meshSystem.ProcessCompletedChunkMeshes(voxelWorld, clientData, assetManager, CHUNK_MESH_UPLOAD_BUDGET);
}

void ClientWorld::Render(const World& world, int playerId)
{
  if (!initialized) return;
  RenderWorld(world.GetVoxelWorld(), world.GetPlayerSystem(), clientData, assetManager, playerId);
}
