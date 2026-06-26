#pragma once

#include <string>

#include "world/WorldConfig.h"

inline std::string BuildAssetPath(const char* folder, const char* name)
{
  std::string path = "assets/";
  path += folder;
  path += "/";
  path += name;
  return path;
}

inline Color GetColorFromUint8(uint8_t value)
{
  if (value == 0) return BLANK;
  if (value == 255) return WHITE;
  float hue = ((float)(value - 1) / 254.0f) * 360.0f;
  return ColorFromHSV(hue, 0.85f, 1.0f);
}

inline bool AreColorsEqual(Color a, Color b)
{
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

inline float GetFaceLightFactor(int normalX, int normalY, int normalZ)
{
  if (normalZ > 0) return 1.0f;
  if (normalZ < 0) return 0.65f;
  if (normalY != 0) return 0.83f;
  if (normalX != 0) return 0.88f;
  return 0.8f;
}

inline int GetChunkSectionBaseVoxelZ(int sectionIndex)
{
  return sectionIndex * CHUNK_SECTION_HEIGHT;
}

inline int GetChunkSectionVoxelHeight(int sectionIndex)
{
  int remainingHeight = CHUNK_MAX_HEIGHT - GetChunkSectionBaseVoxelZ(sectionIndex);
  return remainingHeight > CHUNK_SECTION_HEIGHT ? CHUNK_SECTION_HEIGHT : remainingHeight;
}

inline bool TryGetChunkSectionCoords(int voxelZ, int* sectionIndex, int* localZ)
{
  if (voxelZ < 0 || voxelZ >= CHUNK_MAX_HEIGHT) return false;
  if (sectionIndex != nullptr) *sectionIndex = voxelZ / CHUNK_SECTION_HEIGHT;
  if (localZ != nullptr) *localZ = voxelZ % CHUNK_SECTION_HEIGHT;
  return true;
}

inline void GetPlayerBounds(Vector3 playerPosition, Vector3* minBounds, Vector3* maxBounds)
{
  *minBounds = {playerPosition.x - PLAYER_RADIUS, playerPosition.y - PLAYER_RADIUS, playerPosition.z};
  *maxBounds = {playerPosition.x + PLAYER_RADIUS, playerPosition.y + PLAYER_RADIUS, playerPosition.z + PLAYER_HEIGHT};
}

inline int FloorDiv(int value, int divisor)
{
  int quotient = value / divisor;
  int remainder = value % divisor;
  if (remainder != 0 && ((remainder < 0) != (divisor < 0))) quotient -= 1;
  return quotient;
}

inline int PositiveModulo(int value, int divisor)
{
  int result = value % divisor;
  return result < 0 ? result + divisor : result;
}

inline unsigned int HashChunkCoords(int chunkX, int chunkY)
{
  unsigned int x = (unsigned int)chunkX * 73856093u;
  unsigned int y = (unsigned int)chunkY * 19349663u;
  return (x ^ y) % CHUNK_LOOKUP_CAPACITY;
}
