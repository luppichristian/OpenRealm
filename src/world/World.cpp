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
  voxelWorld.Seed();
  initialized = true;
}

void World::Shutdown()
{
  if (!initialized) return;

  voxelWorld.Shutdown();
  eventQueue.Clear();
  playerSystem.Initialize();
  initialized = false;
}

void World::SendEvent(const WorldEvent& event)
{
  if (!initialized) return;
  eventQueue.Enqueue(event);
}

void World::Update(float frameTime)
{
  if (!initialized) return;
  playerSystem.ResetFrameInputs();

  for (int i = 0; i < eventQueue.Count(); i++)
  {
    playerSystem.ProcessEvent(eventQueue.Get(i));
  }
  eventQueue.Clear();

  playerSystem.Update(frameTime, voxelWorld);
}

VoxelWorld& World::GetVoxelWorld()
{
  return voxelWorld;
}

const VoxelWorld& World::GetVoxelWorld() const
{
  return voxelWorld;
}

PlayerSystem& World::GetPlayerSystem()
{
  return playerSystem;
}

const PlayerSystem& World::GetPlayerSystem() const
{
  return playerSystem;
}
