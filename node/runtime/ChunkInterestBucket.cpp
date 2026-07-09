#include "ChunkInterestBucket.h"

bool ChunkInterestBucket::RegisterInterest(const RuntimePeerAddress& peerAddress, const RuntimeWorldPosition& position, const RuntimeInterestArea& interestArea)
{
  for (ChunkInterestState& state : interests)
  {
    if (!RuntimePeerAddressEquals(state.peerAddress, peerAddress)) continue;
    state.position = position;
    state.interestArea = interestArea;
    return true;
  }

  if (interests.size() >= maxInterests) return false;
  interests.push_back({
      .peerAddress = peerAddress,
      .position = position,
      .interestArea = interestArea,
  });
  return true;
}

size_t ChunkInterestBucket::GetCount() const
{
  return interests.size();
}

const ChunkInterestState* ChunkInterestBucket::FindByPeerAddress(const RuntimePeerAddress& peerAddress) const
{
  for (const ChunkInterestState& state : interests)
  {
    if (RuntimePeerAddressEquals(state.peerAddress, peerAddress)) return &state;
  }
  return nullptr;
}

std::vector<RuntimePeerAddress> ChunkInterestBucket::BuildInterestedPeerAddresses(
    const ActiveNodeBucket& activeNodes,
    const RuntimePeerAddress& senderPeerAddress,
    const RuntimeWorldPosition& senderPosition,
    const RuntimeInterestArea& senderInterestArea) const
{
  std::vector<RuntimePeerAddress> peerAddresses = {};
  for (const ChunkInterestState& state : interests)
  {
    if (RuntimePeerAddressEquals(state.peerAddress, senderPeerAddress)) continue;
    if (!RuntimeInterestAreasOverlap(senderPosition, senderInterestArea, state.position, state.interestArea)) continue;
    const ActiveNodeState* activeNode = activeNodes.FindByPeerAddress(state.peerAddress);
    if (activeNode == nullptr) continue;
    peerAddresses.push_back(activeNode->peerAddress);
  }
  return peerAddresses;
}
