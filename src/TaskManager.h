#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "Utils.h"

class TaskManager : public NonCopyable
{
 public:
  using Task = std::function<void()>;

  TaskManager() = default;
  ~TaskManager();

  bool Start(int workerCount);
  void Stop();
  bool IsRunning() const;
  bool Enqueue(Task task, int maxPendingTasks = 0);

 private:
  void WorkerMain();

  bool running = false;
  mutable std::mutex mutex;
  std::condition_variable taskAvailable;
  std::deque<Task> pendingTasks;
  std::vector<std::thread> workers;
};
