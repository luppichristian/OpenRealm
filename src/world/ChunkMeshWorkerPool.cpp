#include "ChunkMeshWorkerPool.h"

#include "ChunkMesher.h"

ChunkMeshWorkerPool::~ChunkMeshWorkerPool()
{
  Stop();
}

bool ChunkMeshWorkerPool::Start()
{
  Stop();
  running = true;

  try
  {
    workers.reserve(MESH_WORKER_COUNT);
    for (int workerIndex = 0; workerIndex < MESH_WORKER_COUNT; workerIndex++)
    {
      workers.emplace_back(&ChunkMeshWorkerPool::WorkerMain, this);
    }
  }
  catch (...)
  {
    Stop();
    return false;
  }

  return true;
}

void ChunkMeshWorkerPool::Stop()
{
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!running && workers.empty() && completedJobs.empty() && pendingJobs.empty()) return;
    running = false;
  }

  jobAvailable.notify_all();

  for (std::thread& worker : workers)
  {
    if (worker.joinable()) worker.join();
  }
  workers.clear();

  pendingJobs.clear();
  completedJobs.clear();
}

bool ChunkMeshWorkerPool::IsRunning() const
{
  std::lock_guard<std::mutex> lock(mutex);
  return running;
}

bool ChunkMeshWorkerPool::Enqueue(const ChunkMeshJob& job)
{
  std::lock_guard<std::mutex> lock(mutex);
  if (!running) return false;
  if ((int)pendingJobs.size() >= MAX_MESH_JOBS) return false;

  pendingJobs.push_back(job);
  jobAvailable.notify_one();
  return true;
}

bool ChunkMeshWorkerPool::PopCompleted(ChunkMeshJobResult* result)
{
  std::lock_guard<std::mutex> lock(mutex);
  if (completedJobs.empty()) return false;

  *result = std::move(completedJobs.front());
  completedJobs.pop_front();
  return true;
}

void ChunkMeshWorkerPool::WorkerMain()
{
  for (;;)
  {
    ChunkMeshJob job = {};

    {
      std::unique_lock<std::mutex> lock(mutex);
      jobAvailable.wait(lock, [&]
                        { return !running || !pendingJobs.empty(); });
      if (!running && pendingJobs.empty()) return;

      job = pendingJobs.front();
      pendingJobs.pop_front();
    }

    ChunkMeshJobResult result = ChunkMesher::Build(job);

    std::lock_guard<std::mutex> lock(mutex);
    completedJobs.push_back(std::move(result));
  }
}
