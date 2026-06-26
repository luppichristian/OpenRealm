#pragma once

#include <array>

#include "WorldConfig.h"
#include "WorldEvent.h"

class WorldEventQueue
{
 public:
  bool Enqueue(const WorldEvent& event);
  void Clear();
  int Count() const;
  const WorldEvent& Get(int index) const;

 private:
  std::array<WorldEvent, MAX_WORLD_EVENTS> events = {};
  int count = 0;
};
