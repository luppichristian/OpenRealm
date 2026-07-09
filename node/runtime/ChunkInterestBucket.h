#pragma once

#include "../Utils.h"
#include "ActiveNodeBucket.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct ChunkInterestState
{
  RuntimePeerAddress peerAddress = {};
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

  bool RegisterInterest(const RuntimePeerAddress& peerAddress, const RuntimeWorldPosition& position, const RuntimeInterestArea& interestArea);
  size_t GetCount() const;
  const ChunkInterestState* FindByPeerAddress(const RuntimePeerAddress& peerAddress) const;
  std::vector<RuntimePeerAddress> BuildInterestedPeerAddresses(
      const ActiveNodeBucket& activeNodes,
      const RuntimePeerAddress& senderPeerAddress,
      const RuntimeWorldPosition& senderPosition,
      const RuntimeInterestArea& senderInterestArea) const;

 private:
  size_t maxInterests = 64;
  std::vector<ChunkInterestState> interests = {};
};
