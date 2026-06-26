#include "World.h"

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
  assets.Initialize();

  if (!meshSystem.Start())
  {
    TraceLog(LOG_WARNING, "Failed to start mesh worker threads; voxel mesh updates will be disabled.");
  }

  voxelWorld.Seed();
  initialized_ = true;
}

void World::Shutdown()
{
  if (!initialized_) return;

  meshSystem.Stop();
  voxelWorld.Shutdown();
  assets.Shutdown();
  eventQueue.Clear();
  playerSystem.Initialize();
  initialized_ = false;
}

void World::SendEvent(const WorldEvent& event)
{
  if (!initialized_) return;
  eventQueue.Enqueue(event);
}

void World::Update()
{
  if (!initialized_) return;

  float frameTime = GetFrameTime();
  playerSystem.ResetFrameInputs();

  for (int i = 0; i < eventQueue.Count(); i++)
  {
    playerSystem.ProcessEvent(eventQueue.Get(i));
  }
  eventQueue.Clear();

  playerSystem.Update(frameTime, voxelWorld, assets);
  meshSystem.QueueDirtyChunkSectionMeshes(voxelWorld, CHUNK_MESH_QUEUE_BUDGET);
  meshSystem.ProcessCompletedChunkMeshes(voxelWorld, assets, CHUNK_MESH_UPLOAD_BUDGET);
}

void World::Render(int playerId)
{
  if (!initialized_) return;
  assets.Initialize();
  renderer.Render(voxelWorld, playerSystem, assets, playerId);
}
