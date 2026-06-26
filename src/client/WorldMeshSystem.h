#pragma once

#include "../world/VoxelWorld.h"
#include "AssetManager.h"
#include "ChunkMeshWorkerPool.h"
#include "WorldClientData.h"

class WorldMeshSystem
{
 public:
  ~WorldMeshSystem();

  bool Start();
  void Stop(WorldClientData& clientData);
  void QueueDirtyChunkSectionMeshes(VoxelWorld& voxelWorld, WorldClientData& clientData, int maxSectionsToQueue);
  void ProcessCompletedChunkMeshes(VoxelWorld& voxelWorld, WorldClientData& clientData, AssetManager& assetManager, int maxUploads);

 private:
  static void UnloadChunkRenderData(ClientChunkSectionState* sectionState);
  static void ResetClientData(WorldClientData& clientData);
  ChunkMeshJob CaptureChunkSectionMeshJob(const VoxelWorld& voxelWorld, const VoxelChunk& chunk, int sectionIndex) const;
  bool EnqueueChunkMeshJob(const VoxelWorld& voxelWorld, const VoxelChunk& chunk, int sectionIndex);
  void ApplyCompletedChunkMesh(VoxelWorld& voxelWorld, WorldClientData& clientData, AssetManager& assetManager, ChunkMeshJobResult* result);

  ChunkMeshWorkerPool workerPool;
};
