#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "ChunkMeshJobResult.h"
#include "../world/WorldConfig.h"

struct ChunkMeshJob
{
  int chunkX = 0;
  int chunkY = 0;
  int sectionIndex = 0;
  int baseVoxelZ = 0;
  int sectionHeight = 0;
  unsigned int revision = 0;
  uint8_t voxels[SECTION_SNAPSHOT_SIZE_Z][SECTION_SNAPSHOT_SIZE_Y][SECTION_SNAPSHOT_SIZE_X] = {};
};

class ChunkMeshWorkerPool
{
 public:
  ChunkMeshWorkerPool() = default;
  ~ChunkMeshWorkerPool();

  ChunkMeshWorkerPool(const ChunkMeshWorkerPool&) = delete;
  ChunkMeshWorkerPool& operator=(const ChunkMeshWorkerPool&) = delete;

  bool Start();
  void Stop();
  bool IsRunning() const;
  bool Enqueue(const ChunkMeshJob& job);
  bool PopCompleted(ChunkMeshJobResult* result);

 private:
  void WorkerMain();

  bool running = false;
  mutable std::mutex mutex;
  std::condition_variable jobAvailable;
  std::deque<ChunkMeshJob> pendingJobs;
  std::deque<ChunkMeshJobResult> completedJobs;
  std::vector<std::thread> workers;
};
