#include "ChunkInterestBucket.h"

#include "../world/WorldEvent.h"

#include <cmath>

bool IsChunkInsideInterest(const ChunkInterestState& interest, int chunkX, int chunkY)
{
  const int deltaX = chunkX - interest.chunkX;
  const int deltaY = chunkY - interest.chunkY;
  return std::abs(deltaX) <= (int)interest.radius && std::abs(deltaY) <= (int)interest.radius;
}

bool ChunkInterestBucket::RegisterInterest(const ChunkInterestPacketData& chunkInterest)
{
  for (ChunkInterestState& interest : interests)
  {
    if (interest.nodeId != chunkInterest.nodeId) continue;

    interest.chunkX = chunkInterest.chunkX;
    interest.chunkY = chunkInterest.chunkY;
    interest.radius = chunkInterest.radius;
    interest.acceptedPackets += 1;
    return true;
  }

  if (interests.size() >= maxInterests) return false;

  ChunkInterestState interest = {};
  interest.nodeId = chunkInterest.nodeId;
  interest.chunkX = chunkInterest.chunkX;
  interest.chunkY = chunkInterest.chunkY;
  interest.radius = chunkInterest.radius;
  interest.acceptedPackets = 1;
  interests.push_back(interest);
  return true;
}

size_t ChunkInterestBucket::GetCount() const
{
  return interests.size();
}

const ChunkInterestState* ChunkInterestBucket::FindByNodeId(uint32_t nodeId) const
{
  for (const ChunkInterestState& interest : interests)
  {
    if (interest.nodeId == nodeId) return &interest;
  }
  return nullptr;
}

std::vector<RuntimePeerAddress> ChunkInterestBucket::BuildInterestedPeerAddresses(
    const ActiveNodeBucket& activeNodes,
    uint32_t senderNodeId,
    int chunkX,
    int chunkY,
    size_t maxRecipients
) const
{
  std::vector<RuntimePeerAddress> peerAddresses = {};

  for (const ChunkInterestState& interest : interests)
  {
    if (interest.nodeId == senderNodeId) continue;
    if (!IsChunkInsideInterest(interest, chunkX, chunkY)) continue;

    const ActiveNodeState* activeNode = activeNodes.FindByNodeId(interest.nodeId);
    if (activeNode == nullptr) continue;
    peerAddresses.push_back(activeNode->peerAddress);
    if (peerAddresses.size() >= maxRecipients) break;
  }

  return peerAddresses;
}

bool ChunkInterestBucket::TryResolveWorldEventChunk(const WorldEventPacketData& worldEvent, int* chunkX, int* chunkY) const
{
  if (chunkX == nullptr || chunkY == nullptr) return false;

  if (worldEvent.event.type == (uint8_t)WorldEventType::Place)
  {
    *chunkX = FloorDiv(worldEvent.event.voxelX, CHUNK_SIZE_XZ);
    *chunkY = FloorDiv(worldEvent.event.voxelY, CHUNK_SIZE_XZ);
    return true;
  }

  if (worldEvent.event.type == (uint8_t)WorldEventType::Spawn)
  {
    const int voxelX = (int)floorf(worldEvent.event.playerX / BLOCK_GRID_SIZE);
    const int voxelY = (int)floorf(worldEvent.event.playerY / BLOCK_GRID_SIZE);
    *chunkX = FloorDiv(voxelX, CHUNK_SIZE_XZ);
    *chunkY = FloorDiv(voxelY, CHUNK_SIZE_XZ);
    return true;
  }

  const ChunkInterestState* interest = FindByNodeId(worldEvent.nodeId);
  if (interest == nullptr) return false;

  *chunkX = interest->chunkX;
  *chunkY = interest->chunkY;
  return true;
}
