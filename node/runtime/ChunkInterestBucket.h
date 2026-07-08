#pragma once

#include "ActiveNodeBucket.h"
#include "../Utils.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct ChunkInterestState
{
  uint32_t nodeId = 0;
  RuntimeWorldPosition position = {};
  RuntimeInterestArea interestArea = {};
};

class ChunkInterestBucket
{
 public:
  explicit ChunkInterestBucket(size_t maxInterests = 64)
      : maxInterests(maxInterests)
  {
  }

  bool RegisterInterest(uint32_t nodeId, const RuntimeWorldPosition& position, const RuntimeInterestArea& interestArea);
  size_t GetCount() const;
  const ChunkInterestState* FindByNodeId(uint32_t nodeId) const;
  std::vector<RuntimePeerAddress> BuildInterestedPeerAddresses(
      const ActiveNodeBucket& activeNodes,
      uint32_t senderNodeId,
      const RuntimeWorldPosition& senderPosition,
      const RuntimeInterestArea& senderInterestArea) const;

 private:
  size_t maxInterests = 64;
  std::vector<ChunkInterestState> interests = {};
};
