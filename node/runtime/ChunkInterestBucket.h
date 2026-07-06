#pragma once

#include "ActiveNodeBucket.h"
#include "../Utils.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct ChunkInterestState
{
  uint32_t nodeId = 0;
  int chunkX = 0;
  int chunkY = 0;
  uint32_t radius = 0;
  uint32_t acceptedPackets = 0;
};

class ChunkInterestBucket
{
 public:
  explicit ChunkInterestBucket(size_t maxInterests = 64)
      : maxInterests(maxInterests)
  {
  }

  bool RegisterInterest(const ChunkInterestPacketData& chunkInterest);

  size_t GetCount() const;
  const ChunkInterestState* FindByNodeId(uint32_t nodeId) const;
  std::vector<RuntimePeerAddress> BuildInterestedPeerAddresses(
      const ActiveNodeBucket& activeNodes,
      uint32_t senderNodeId,
      int chunkX,
      int chunkY,
      size_t maxRecipients = SIZE_MAX
  ) const;
  bool TryResolveWorldEventChunk(const WorldEventPacketData& worldEvent, int* chunkX, int* chunkY) const;

 private:
  size_t maxInterests = 64;
  std::vector<ChunkInterestState> interests = {};
};

bool IsChunkInsideInterest(const ChunkInterestState& interest, int chunkX, int chunkY);
