#include "WorldEventQueue.h"

bool WorldEventQueue::Enqueue(const WorldEvent& event)
{
  if (count >= MAX_WORLD_EVENTS) return false;
  events[count] = event;
  count += 1;
  return true;
}

void WorldEventQueue::Clear()
{
  count = 0;
}

int WorldEventQueue::Count() const
{
  return count;
}

const WorldEvent& WorldEventQueue::Get(int index) const
{
  return events[index];
}
