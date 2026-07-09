#pragma once

#include <cstdint>
#include <string>

struct RuntimeWorldPosition
{
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct RuntimeInterestArea
{
  float radiusX = 96.0f;
  float radiusY = 96.0f;
  float radiusZ = 64.0f;
};

inline float ComputeRuntimeAxisDistance(float a, float b)
{
  const float delta = a - b;
  return delta < 0.0f ? -delta : delta;
}

inline float ComputeRuntimeWorldDistanceSquared(const RuntimeWorldPosition& a, const RuntimeWorldPosition& b)
{
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return dx * dx + dy * dy + dz * dz;
}

inline bool RuntimeInterestAreasOverlap(
    const RuntimeWorldPosition& aPosition,
    const RuntimeInterestArea& aArea,
    const RuntimeWorldPosition& bPosition,
    const RuntimeInterestArea& bArea)
{
  return ComputeRuntimeAxisDistance(aPosition.x, bPosition.x) <= (aArea.radiusX + bArea.radiusX) && ComputeRuntimeAxisDistance(aPosition.y, bPosition.y) <= (aArea.radiusY + bArea.radiusY) && ComputeRuntimeAxisDistance(aPosition.z, bPosition.z) <= (aArea.radiusZ + bArea.radiusZ);
}

inline std::string DescribeRuntimeWorldPosition(const RuntimeWorldPosition& position)
{
  return "(" + std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + ")";
}
