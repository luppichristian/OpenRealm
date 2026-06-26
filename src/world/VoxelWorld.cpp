#include "VoxelWorld.h"

int VoxelWorld::GetChunkSectionBaseVoxelZ(int sectionIndex)
{
  return sectionIndex * CHUNK_SECTION_HEIGHT;
}

int VoxelWorld::GetChunkSectionVoxelHeight(int sectionIndex)
{
  int remainingHeight = CHUNK_MAX_HEIGHT - GetChunkSectionBaseVoxelZ(sectionIndex);
  return remainingHeight > CHUNK_SECTION_HEIGHT ? CHUNK_SECTION_HEIGHT : remainingHeight;
}

bool VoxelWorld::TryGetChunkSectionCoords(int voxelZ, int* sectionIndex, int* localZ)
{
  if (voxelZ < 0 || voxelZ >= CHUNK_MAX_HEIGHT) return false;
  if (sectionIndex != nullptr) *sectionIndex = voxelZ / CHUNK_SECTION_HEIGHT;
  if (localZ != nullptr) *localZ = voxelZ % CHUNK_SECTION_HEIGHT;
  return true;
}

int VoxelWorld::FloorDiv(int value, int divisor)
{
  int quotient = value / divisor;
  int remainder = value % divisor;
  if (remainder != 0 && ((remainder < 0) != (divisor < 0))) quotient -= 1;
  return quotient;
}

int VoxelWorld::PositiveModulo(int value, int divisor)
{
  int result = value % divisor;
  return result < 0 ? result + divisor : result;
}

unsigned int VoxelWorld::HashChunkCoords(int chunkX, int chunkY)
{
  unsigned int x = (unsigned int)chunkX * 73856093u;
  unsigned int y = (unsigned int)chunkY * 19349663u;
  return (x ^ y) % CHUNK_LOOKUP_CAPACITY;
}

void VoxelWorld::Initialize()
{
  Shutdown();
  ResetChunkLookup();
}

void VoxelWorld::Shutdown()
{
  for (int chunkIndex = 0; chunkIndex < chunkCount; chunkIndex++)
  {
    for (int sectionIndex = 0; sectionIndex < CHUNK_SECTION_COUNT; sectionIndex++)
    {
      VoxelChunkSection& section = chunks[chunkIndex].sections[sectionIndex];
      if (section.uploaded)
      {
        UnloadModel(section.model);
        section.uploaded = false;
      }
    }
  }

  chunks.fill({});
  chunkLookup.fill(-1);
  chunkCount = 0;
}

void VoxelWorld::Seed()
{
}

void VoxelWorld::ResetChunkLookup()
{
  chunkLookup.fill(-1);
}

int VoxelWorld::FindChunkIndex(int chunkX, int chunkY) const
{
  unsigned int slot = HashChunkCoords(chunkX, chunkY);
  for (int probe = 0; probe < CHUNK_LOOKUP_CAPACITY; probe++)
  {
    int index = chunkLookup[slot];
    if (index < 0) return -1;
    if (chunks[index].chunkX == chunkX && chunks[index].chunkY == chunkY) return index;
    slot = (slot + 1u) % CHUNK_LOOKUP_CAPACITY;
  }

  return -1;
}

bool VoxelWorld::InsertChunkLookup(int chunkIndex)
{
  unsigned int slot = HashChunkCoords(chunks[chunkIndex].chunkX, chunks[chunkIndex].chunkY);
  for (int probe = 0; probe < CHUNK_LOOKUP_CAPACITY; probe++)
  {
    if (chunkLookup[slot] < 0)
    {
      chunkLookup[slot] = chunkIndex;
      return true;
    }

    slot = (slot + 1u) % CHUNK_LOOKUP_CAPACITY;
  }

  return false;
}

VoxelChunk* VoxelWorld::FindChunk(int chunkX, int chunkY)
{
  int chunkIndex = FindChunkIndex(chunkX, chunkY);
  return chunkIndex >= 0 ? &chunks[chunkIndex] : nullptr;
}

const VoxelChunk* VoxelWorld::FindChunk(int chunkX, int chunkY) const
{
  int chunkIndex = FindChunkIndex(chunkX, chunkY);
  return chunkIndex >= 0 ? &chunks[chunkIndex] : nullptr;
}

VoxelChunkSection* VoxelWorld::FindChunkSectionByCoords(int chunkX, int chunkY, int sectionIndex)
{
  if (sectionIndex < 0 || sectionIndex >= CHUNK_SECTION_COUNT) return nullptr;
  VoxelChunk* chunk = FindChunk(chunkX, chunkY);
  if (chunk == nullptr) return nullptr;
  return &chunk->sections[sectionIndex];
}

const VoxelChunkSection* VoxelWorld::FindChunkSectionByCoords(int chunkX, int chunkY, int sectionIndex) const
{
  if (sectionIndex < 0 || sectionIndex >= CHUNK_SECTION_COUNT) return nullptr;
  const VoxelChunk* chunk = FindChunk(chunkX, chunkY);
  if (chunk == nullptr) return nullptr;
  return &chunk->sections[sectionIndex];
}

void VoxelWorld::MarkChunkSectionDirty(VoxelChunkSection* section)
{
  if (section == nullptr) return;
  section->dirty = true;
  section->revision += 1;
}

void VoxelWorld::MarkChunkSectionDirtyByCoords(int chunkX, int chunkY, int sectionIndex)
{
  MarkChunkSectionDirty(FindChunkSectionByCoords(chunkX, chunkY, sectionIndex));
}

VoxelChunk* VoxelWorld::GetOrCreateChunk(int chunkX, int chunkY)
{
  VoxelChunk* chunk = FindChunk(chunkX, chunkY);
  if (chunk != nullptr) return chunk;
  if (chunkCount >= MAX_VOXEL_CHUNKS) return nullptr;

  int chunkIndex = chunkCount++;
  chunk = &chunks[chunkIndex];
  *chunk = {};
  chunk->chunkX = chunkX;
  chunk->chunkY = chunkY;

  if (!InsertChunkLookup(chunkIndex))
  {
    chunkCount -= 1;
    *chunk = {};
    return nullptr;
  }

  return chunk;
}

uint8_t VoxelWorld::GetVoxel(int voxelX, int voxelY, int voxelZ) const
{
  int sectionIndex = 0;
  int localZ = 0;
  if (!TryGetChunkSectionCoords(voxelZ, &sectionIndex, &localZ)) return 0;

  int chunkX = FloorDiv(voxelX, CHUNK_SIZE_XZ);
  int chunkY = FloorDiv(voxelY, CHUNK_SIZE_XZ);
  int localX = PositiveModulo(voxelX, CHUNK_SIZE_XZ);
  int localY = PositiveModulo(voxelY, CHUNK_SIZE_XZ);
  const VoxelChunk* chunk = FindChunk(chunkX, chunkY);

  if (chunk == nullptr) return 0;
  return chunk->sections[sectionIndex].voxels[localZ][localY][localX];
}

void VoxelWorld::MarkVoxelNeighborsDirty(int voxelX, int voxelY, int voxelZ)
{
  int sectionIndex = 0;
  int localZ = 0;
  if (!TryGetChunkSectionCoords(voxelZ, &sectionIndex, &localZ)) return;

  int chunkX = FloorDiv(voxelX, CHUNK_SIZE_XZ);
  int chunkY = FloorDiv(voxelY, CHUNK_SIZE_XZ);
  int localX = PositiveModulo(voxelX, CHUNK_SIZE_XZ);
  int localY = PositiveModulo(voxelY, CHUNK_SIZE_XZ);
  int sectionHeight = GetChunkSectionVoxelHeight(sectionIndex);

  MarkChunkSectionDirtyByCoords(chunkX, chunkY, sectionIndex);
  if (localX == 0) MarkChunkSectionDirtyByCoords(chunkX - 1, chunkY, sectionIndex);
  if (localX == CHUNK_SIZE_XZ - 1) MarkChunkSectionDirtyByCoords(chunkX + 1, chunkY, sectionIndex);
  if (localY == 0) MarkChunkSectionDirtyByCoords(chunkX, chunkY - 1, sectionIndex);
  if (localY == CHUNK_SIZE_XZ - 1) MarkChunkSectionDirtyByCoords(chunkX, chunkY + 1, sectionIndex);
  if (localZ == 0) MarkChunkSectionDirtyByCoords(chunkX, chunkY, sectionIndex - 1);
  if (localZ == sectionHeight - 1) MarkChunkSectionDirtyByCoords(chunkX, chunkY, sectionIndex + 1);
}

void VoxelWorld::SetVoxel(int voxelX, int voxelY, int voxelZ, uint8_t voxelValue)
{
  int sectionIndex = 0;
  int localZ = 0;
  if (!TryGetChunkSectionCoords(voxelZ, &sectionIndex, &localZ)) return;

  int chunkX = FloorDiv(voxelX, CHUNK_SIZE_XZ);
  int chunkY = FloorDiv(voxelY, CHUNK_SIZE_XZ);
  int localX = PositiveModulo(voxelX, CHUNK_SIZE_XZ);
  int localY = PositiveModulo(voxelY, CHUNK_SIZE_XZ);
  VoxelChunk* chunk = GetOrCreateChunk(chunkX, chunkY);
  if (chunk == nullptr) return;

  VoxelChunkSection* section = &chunk->sections[sectionIndex];
  if (section->voxels[localZ][localY][localX] == voxelValue) return;

  section->voxels[localZ][localY][localX] = voxelValue;
  MarkVoxelNeighborsDirty(voxelX, voxelY, voxelZ);
}

bool VoxelWorld::IsVoxelFilled(int voxelX, int voxelY, int voxelZ) const
{
  return GetVoxel(voxelX, voxelY, voxelZ) != 0;
}

Color VoxelWorld::GetVoxelDisplayColor(uint8_t voxelValue) const
{
  if (voxelValue == 0) return BLANK;
  if (voxelValue == 255) return WHITE;

  float hue = ((float)(voxelValue - 1) / 254.0f) * 360.0f;
  return ColorFromHSV(hue, 0.85f, 1.0f);
}

void VoxelWorld::GetPlayerBounds(Vector3 playerPosition, Vector3* minBounds, Vector3* maxBounds)
{
  *minBounds = {playerPosition.x - PLAYER_RADIUS, playerPosition.y - PLAYER_RADIUS, playerPosition.z};
  *maxBounds = {playerPosition.x + PLAYER_RADIUS, playerPosition.y + PLAYER_RADIUS, playerPosition.z + PLAYER_HEIGHT};
}

bool VoxelWorld::IsAabbCollidingWithVoxels(Vector3 minBounds, Vector3 maxBounds) const
{
  const float epsilon = 0.001f;
  int minVoxelX = (int)floorf(minBounds.x / BLOCK_GRID_SIZE);
  int maxVoxelX = (int)floorf((maxBounds.x - epsilon) / BLOCK_GRID_SIZE);
  int minVoxelY = (int)floorf(minBounds.y / BLOCK_GRID_SIZE);
  int maxVoxelY = (int)floorf((maxBounds.y - epsilon) / BLOCK_GRID_SIZE);
  int minVoxelZ = (int)floorf(minBounds.z / BLOCK_GRID_SIZE);
  int maxVoxelZ = (int)floorf((maxBounds.z - epsilon) / BLOCK_GRID_SIZE);

  for (int voxelZ = minVoxelZ; voxelZ <= maxVoxelZ; voxelZ++)
  {
    for (int voxelY = minVoxelY; voxelY <= maxVoxelY; voxelY++)
    {
      for (int voxelX = minVoxelX; voxelX <= maxVoxelX; voxelX++)
      {
        if (GetVoxel(voxelX, voxelY, voxelZ) != 0) return true;
      }
    }
  }

  return false;
}

bool VoxelWorld::IsPlayerColliding(Vector3 playerPosition) const
{
  Vector3 minBounds;
  Vector3 maxBounds;
  GetPlayerBounds(playerPosition, &minBounds, &maxBounds);
  return IsAabbCollidingWithVoxels(minBounds, maxBounds);
}

float VoxelWorld::ClampMovementAgainstVoxels(Vector3 minBounds, Vector3 maxBounds, int axisIndex, float delta, bool* collided) const
{
  const float epsilon = 0.001f;
  if (delta == 0.0f) return 0.0f;

  float allowedDelta = delta;

  if (axisIndex == 0)
  {
    int minVoxelY = (int)floorf(minBounds.y / BLOCK_GRID_SIZE);
    int maxVoxelY = (int)floorf((maxBounds.y - epsilon) / BLOCK_GRID_SIZE);
    int minVoxelZ = (int)floorf(minBounds.z / BLOCK_GRID_SIZE);
    int maxVoxelZ = (int)floorf((maxBounds.z - epsilon) / BLOCK_GRID_SIZE);

    if (delta > 0.0f)
    {
      int startVoxelX = (int)floorf((maxBounds.x - epsilon) / BLOCK_GRID_SIZE) + 1;
      int endVoxelX = (int)floorf((maxBounds.x + delta - epsilon) / BLOCK_GRID_SIZE);
      for (int voxelX = startVoxelX; voxelX <= endVoxelX; voxelX++)
      {
        for (int voxelZ = minVoxelZ; voxelZ <= maxVoxelZ; voxelZ++)
        {
          for (int voxelY = minVoxelY; voxelY <= maxVoxelY; voxelY++)
          {
            if (GetVoxel(voxelX, voxelY, voxelZ) == 0) continue;
            float voxelMin = (float)voxelX * BLOCK_GRID_SIZE;
            float maxAllowed = voxelMin - maxBounds.x;
            if (maxAllowed < allowedDelta) allowedDelta = maxAllowed;
            if (collided != nullptr) *collided = true;
            return allowedDelta > 0.0f ? allowedDelta : 0.0f;
          }
        }
      }
    }
    else
    {
      int startVoxelX = (int)floorf(minBounds.x / BLOCK_GRID_SIZE) - 1;
      int endVoxelX = (int)floorf((minBounds.x + delta) / BLOCK_GRID_SIZE);
      for (int voxelX = startVoxelX; voxelX >= endVoxelX; voxelX--)
      {
        for (int voxelZ = minVoxelZ; voxelZ <= maxVoxelZ; voxelZ++)
        {
          for (int voxelY = minVoxelY; voxelY <= maxVoxelY; voxelY++)
          {
            if (GetVoxel(voxelX, voxelY, voxelZ) == 0) continue;
            float voxelMax = ((float)voxelX + 1.0f) * BLOCK_GRID_SIZE;
            float minAllowed = voxelMax - minBounds.x;
            if (minAllowed > allowedDelta) allowedDelta = minAllowed;
            if (collided != nullptr) *collided = true;
            return allowedDelta < 0.0f ? allowedDelta : 0.0f;
          }
        }
      }
    }
  }
  else if (axisIndex == 1)
  {
    int minVoxelX = (int)floorf(minBounds.x / BLOCK_GRID_SIZE);
    int maxVoxelX = (int)floorf((maxBounds.x - epsilon) / BLOCK_GRID_SIZE);
    int minVoxelZ = (int)floorf(minBounds.z / BLOCK_GRID_SIZE);
    int maxVoxelZ = (int)floorf((maxBounds.z - epsilon) / BLOCK_GRID_SIZE);

    if (delta > 0.0f)
    {
      int startVoxelY = (int)floorf((maxBounds.y - epsilon) / BLOCK_GRID_SIZE) + 1;
      int endVoxelY = (int)floorf((maxBounds.y + delta - epsilon) / BLOCK_GRID_SIZE);
      for (int voxelY = startVoxelY; voxelY <= endVoxelY; voxelY++)
      {
        for (int voxelZ = minVoxelZ; voxelZ <= maxVoxelZ; voxelZ++)
        {
          for (int voxelX = minVoxelX; voxelX <= maxVoxelX; voxelX++)
          {
            if (GetVoxel(voxelX, voxelY, voxelZ) == 0) continue;
            float voxelMin = (float)voxelY * BLOCK_GRID_SIZE;
            float maxAllowed = voxelMin - maxBounds.y;
            if (maxAllowed < allowedDelta) allowedDelta = maxAllowed;
            if (collided != nullptr) *collided = true;
            return allowedDelta > 0.0f ? allowedDelta : 0.0f;
          }
        }
      }
    }
    else
    {
      int startVoxelY = (int)floorf(minBounds.y / BLOCK_GRID_SIZE) - 1;
      int endVoxelY = (int)floorf((minBounds.y + delta) / BLOCK_GRID_SIZE);
      for (int voxelY = startVoxelY; voxelY >= endVoxelY; voxelY--)
      {
        for (int voxelZ = minVoxelZ; voxelZ <= maxVoxelZ; voxelZ++)
        {
          for (int voxelX = minVoxelX; voxelX <= maxVoxelX; voxelX++)
          {
            if (GetVoxel(voxelX, voxelY, voxelZ) == 0) continue;
            float voxelMax = ((float)voxelY + 1.0f) * BLOCK_GRID_SIZE;
            float minAllowed = voxelMax - minBounds.y;
            if (minAllowed > allowedDelta) allowedDelta = minAllowed;
            if (collided != nullptr) *collided = true;
            return allowedDelta < 0.0f ? allowedDelta : 0.0f;
          }
        }
      }
    }
  }
  else
  {
    int minVoxelX = (int)floorf(minBounds.x / BLOCK_GRID_SIZE);
    int maxVoxelX = (int)floorf((maxBounds.x - epsilon) / BLOCK_GRID_SIZE);
    int minVoxelY = (int)floorf(minBounds.y / BLOCK_GRID_SIZE);
    int maxVoxelY = (int)floorf((maxBounds.y - epsilon) / BLOCK_GRID_SIZE);

    if (delta > 0.0f)
    {
      int startVoxelZ = (int)floorf((maxBounds.z - epsilon) / BLOCK_GRID_SIZE) + 1;
      int endVoxelZ = (int)floorf((maxBounds.z + delta - epsilon) / BLOCK_GRID_SIZE);
      for (int voxelZ = startVoxelZ; voxelZ <= endVoxelZ; voxelZ++)
      {
        for (int voxelY = minVoxelY; voxelY <= maxVoxelY; voxelY++)
        {
          for (int voxelX = minVoxelX; voxelX <= maxVoxelX; voxelX++)
          {
            if (GetVoxel(voxelX, voxelY, voxelZ) == 0) continue;
            float voxelMin = (float)voxelZ * BLOCK_GRID_SIZE;
            float maxAllowed = voxelMin - maxBounds.z;
            if (maxAllowed < allowedDelta) allowedDelta = maxAllowed;
            if (collided != nullptr) *collided = true;
            return allowedDelta > 0.0f ? allowedDelta : 0.0f;
          }
        }
      }
    }
    else
    {
      int startVoxelZ = (int)floorf(minBounds.z / BLOCK_GRID_SIZE) - 1;
      int endVoxelZ = (int)floorf((minBounds.z + delta) / BLOCK_GRID_SIZE);
      for (int voxelZ = startVoxelZ; voxelZ >= endVoxelZ; voxelZ--)
      {
        for (int voxelY = minVoxelY; voxelY <= maxVoxelY; voxelY++)
        {
          for (int voxelX = minVoxelX; voxelX <= maxVoxelX; voxelX++)
          {
            if (GetVoxel(voxelX, voxelY, voxelZ) == 0) continue;
            float voxelMax = ((float)voxelZ + 1.0f) * BLOCK_GRID_SIZE;
            float minAllowed = voxelMax - minBounds.z;
            if (minAllowed > allowedDelta) allowedDelta = minAllowed;
            if (collided != nullptr) *collided = true;
            return allowedDelta < 0.0f ? allowedDelta : 0.0f;
          }
        }
      }
    }
  }

  return allowedDelta;
}

CollisionMoveResult VoxelWorld::MovePlayerAlongAxis(Vector3* playerPosition, int axisIndex, float delta) const
{
  CollisionMoveResult result = {};
  if (delta == 0.0f) return result;

  Vector3 minBounds;
  Vector3 maxBounds;
  GetPlayerBounds(*playerPosition, &minBounds, &maxBounds);
  result.movedDelta = ClampMovementAgainstVoxels(minBounds, maxBounds, axisIndex, delta, &result.collided);

  if (axisIndex == 0) playerPosition->x += result.movedDelta;
  else if (axisIndex == 1)
    playerPosition->y += result.movedDelta;
  else
    playerPosition->z += result.movedDelta;

  result.hitFloor = axisIndex == 2 && delta < 0.0f && result.collided;
  return result;
}

void VoxelWorld::ResolvePlayerPenetration(Vector3* playerPosition) const
{
  if (!IsPlayerColliding(*playerPosition)) return;

  const float resolveStep = BLOCK_GRID_SIZE * 0.25f;
  for (int attempt = 0; attempt < 12 && IsPlayerColliding(*playerPosition); attempt++)
  {
    playerPosition->z += resolveStep;
  }
}

VoxelRaycastHit VoxelWorld::Raycast(Vector3 origin, Vector3 direction, float maxDistance) const
{
  VoxelRaycastHit result = {};
  if (Vector3LengthSqr(direction) <= 0.0f) return result;

  Vector3 rayDirection = Vector3Normalize(direction);
  Vector3 currentPoint = origin;
  const float stepDistance = BLOCK_GRID_SIZE * 0.35f;
  float traveledDistance = 0.0f;

  while (traveledDistance <= maxDistance)
  {
    int voxelX = (int)floorf(currentPoint.x / BLOCK_GRID_SIZE);
    int voxelY = (int)floorf(currentPoint.y / BLOCK_GRID_SIZE);
    int voxelZ = (int)floorf(currentPoint.z / BLOCK_GRID_SIZE);
    if (GetVoxel(voxelX, voxelY, voxelZ) != 0)
    {
      result.hit = true;
      result.voxelX = voxelX;
      result.voxelY = voxelY;
      result.voxelZ = voxelZ;

      Vector3 previousPoint = Vector3Subtract(currentPoint, Vector3Scale(rayDirection, stepDistance));
      result.placeX = (int)floorf(previousPoint.x / BLOCK_GRID_SIZE);
      result.placeY = (int)floorf(previousPoint.y / BLOCK_GRID_SIZE);
      result.placeZ = (int)floorf(previousPoint.z / BLOCK_GRID_SIZE);
      return result;
    }

    currentPoint = Vector3Add(currentPoint, Vector3Scale(rayDirection, stepDistance));
    traveledDistance += stepDistance;
  }

  if (rayDirection.z < 0.0f)
  {
    float groundDistance = -origin.z / rayDirection.z;
    if (groundDistance > 0.0f && groundDistance <= maxDistance)
    {
      Vector3 groundHit = Vector3Add(origin, Vector3Scale(rayDirection, groundDistance));
      result.placeOnGround = true;
      result.placeX = (int)floorf(groundHit.x / BLOCK_GRID_SIZE);
      result.placeY = (int)floorf(groundHit.y / BLOCK_GRID_SIZE);
      result.placeZ = 0;
    }
  }

  return result;
}

int VoxelWorld::GetChunkCount() const
{
  return chunkCount;
}

VoxelChunk* VoxelWorld::GetChunkByIndex(int index)
{
  return &chunks[index];
}

const VoxelChunk* VoxelWorld::GetChunkByIndex(int index) const
{
  return &chunks[index];
}
