#include "World.h"

#include "WorldRenderer.h"

World::~World()
{
  Shutdown();
}

void World::Initialize()
{
  Shutdown();
  voxelWorld.Initialize();
  playerSystem.Initialize();
  eventQueue.Clear();
  soundPlayer.Initialize(assetManager);

  if (!meshSystem.Start())
  {
    TraceLog(LOG_WARNING, "Failed to start mesh worker threads; voxel mesh updates will be disabled.");
  }

  voxelWorld.Seed();
  initialized = true;
}

void World::Shutdown()
{
  if (!initialized) return;

  meshSystem.Stop();
  voxelWorld.Shutdown();
  soundPlayer.Shutdown();
  assetManager.Shutdown();
  eventQueue.Clear();
  playerSystem.Initialize();
  initialized = false;
}

void World::SendEvent(const WorldEvent& event)
{
  if (!initialized) return;
  eventQueue.Enqueue(event);
}

void World::Update()
{
  if (!initialized) return;

  float frameTime = GetFrameTime();
  playerSystem.ResetFrameInputs();

  for (int i = 0; i < eventQueue.Count(); i++)
  {
    playerSystem.ProcessEvent(eventQueue.Get(i));
  }
  eventQueue.Clear();

  playerSystem.Update(frameTime, voxelWorld, soundPlayer);
  meshSystem.QueueDirtyChunkSectionMeshes(voxelWorld, CHUNK_MESH_QUEUE_BUDGET);
  meshSystem.ProcessCompletedChunkMeshes(voxelWorld, assetManager, CHUNK_MESH_UPLOAD_BUDGET);
}

void World::Render(int playerId)
{
  if (!initialized) return;
  RenderWorld(voxelWorld, playerSystem, assetManager, playerId);
}
