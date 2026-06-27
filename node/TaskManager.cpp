#include "TaskManager.h"

TaskManager::~TaskManager()
{
  Stop();
}

bool TaskManager::Start(int workerCount)
{
  Stop();
  if (workerCount <= 0) return false;
  running = true;

  try
  {
    workers.reserve(workerCount);
    for (int workerIndex = 0; workerIndex < workerCount; workerIndex++)
    {
      workers.emplace_back(&TaskManager::WorkerMain, this);
    }
  }
  catch (...)
  {
    Stop();
    return false;
  }

  return true;
}

void TaskManager::Stop()
{
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!running && workers.empty() && pendingTasks.empty()) return;
    running = false;
  }

  taskAvailable.notify_all();

  for (std::thread& worker : workers)
  {
    if (worker.joinable()) worker.join();
  }
  workers.clear();
  pendingTasks.clear();
}

bool TaskManager::IsRunning() const
{
  std::lock_guard<std::mutex> lock(mutex);
  return running;
}

bool TaskManager::Enqueue(Task task, int maxPendingTasks)
{
  std::lock_guard<std::mutex> lock(mutex);
  if (!running) return false;
  if (maxPendingTasks > 0 && (int)pendingTasks.size() >= maxPendingTasks) return false;

  pendingTasks.push_back(std::move(task));
  taskAvailable.notify_one();
  return true;
}

void TaskManager::WorkerMain()
{
  for (;;)
  {
    Task task = {};

    {
      std::unique_lock<std::mutex> lock(mutex);
      taskAvailable.wait(lock, [&]
                         { return !running || !pendingTasks.empty(); });
      if (!running && pendingTasks.empty()) return;

      task = std::move(pendingTasks.front());
      pendingTasks.pop_front();
    }

    task();
  }
}
