#pragma once

#include "ChunkMeshWorkerPool.h"
#include "VoxelWorld.h"
#include "WorldAssets.h"

class WorldMeshSystem
{
 public:
  ~WorldMeshSystem();

  bool Start();
  void Stop();
  void QueueDirtyChunkSectionMeshes(VoxelWorld& voxelWorld, int maxSectionsToQueue);
  void ProcessCompletedChunkMeshes(VoxelWorld& voxelWorld, const WorldAssets& assets, int maxUploads);

 private:
  void UnloadChunkRenderData(VoxelChunkSection* section) const;
  ChunkMeshJob CaptureChunkSectionMeshJob(const VoxelWorld& voxelWorld, const VoxelChunk& chunk, int sectionIndex) const;
  bool EnqueueChunkMeshJob(const VoxelWorld& voxelWorld, const VoxelChunk& chunk, int sectionIndex);
  void ApplyCompletedChunkMesh(VoxelWorld& voxelWorld, const WorldAssets& assets, ChunkMeshJobResult* result);

  ChunkMeshWorkerPool workerPool;
};
