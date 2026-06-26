#pragma once

#include "PlayerSystem.h"
#include "VoxelWorld.h"
#include "WorldAssets.h"

class WorldRenderer
{
 public:
  void Render(const VoxelWorld& voxelWorld, const PlayerSystem& playerSystem, const WorldAssets& assets, int playerId) const;

 private:
  bool IsChunkVisible(const BoundingBox* bounds, Vector3 cameraPosition, Vector3 cameraForward, Vector3 cameraRight, Vector3 cameraUp, float aspectRatio) const;
  void DrawVoxelWorld(const VoxelWorld& voxelWorld, Vector3 cameraPosition, Vector3 cameraForward, Vector3 cameraRight, Vector3 cameraUp) const;
};
