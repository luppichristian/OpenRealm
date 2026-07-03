#pragma once

#include <array>

#include "WorldData.h"

class VoxelWorld
{
 public:
  struct MovementSweep
  {
    Vector3 minBounds = {};
    Vector3 maxBounds = {};
    int axisIndex = 0;
    float delta = 0.0f;
  };

  void Initialize();
  void Shutdown();
  void Seed();

  uint8_t GetVoxel(int voxelX, int voxelY, int voxelZ) const;
  void SetVoxel(int voxelX, int voxelY, int voxelZ, uint8_t voxelValue);
  bool IsVoxelFilled(int voxelX, int voxelY, int voxelZ) const;

  bool IsAabbCollidingWithVoxels(Vector3 minBounds, Vector3 maxBounds) const;
  bool IsPlayerColliding(Vector3 playerPosition) const;
  float ClampMovementAgainstVoxels(const MovementSweep& sweep, bool* collided) const;
  CollisionMoveResult MovePlayerAlongAxis(Vector3* playerPosition, int axisIndex, float delta) const;
  void ResolvePlayerPenetration(Vector3* playerPosition) const;
  VoxelRaycastHit Raycast(Vector3 origin, Vector3 direction, float maxDistance) const;

  int GetChunkCount() const;
  VoxelChunk* GetChunkByIndex(int index);
  const VoxelChunk* GetChunkByIndex(int index) const;
  VoxelChunkSection* FindChunkSectionByCoords(int chunkX, int chunkY, int sectionIndex);
  const VoxelChunkSection* FindChunkSectionByCoords(int chunkX, int chunkY, int sectionIndex) const;

 private:
  static bool AreChunkCoordsInBounds(int chunkX, int chunkY);
  void ResetChunkLookup();
  int FindChunkIndex(int chunkX, int chunkY) const;
  bool InsertChunkLookup(int chunkIndex);
  VoxelChunk* FindChunk(int chunkX, int chunkY);
  const VoxelChunk* FindChunk(int chunkX, int chunkY) const;
  void MarkChunkSectionDirty(VoxelChunkSection* section);
  void MarkChunkSectionDirtyByCoords(int chunkX, int chunkY, int sectionIndex);
  void MarkVoxelNeighborsDirty(int voxelX, int voxelY, int voxelZ);
  VoxelChunk* GetOrCreateChunk(int chunkX, int chunkY);

  int chunkCount = 0;
  std::array<VoxelChunk, MAX_VOXEL_CHUNKS> chunks = {};
  std::array<int, CHUNK_LOOKUP_CAPACITY> chunkLookup = {};
};
