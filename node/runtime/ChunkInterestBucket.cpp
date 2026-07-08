#include "ChunkInterestBucket.h"

bool ChunkInterestBucket::RegisterInterest(uint32_t nodeId, const RuntimeWorldPosition& position, const RuntimeInterestArea& interestArea)
{
  for (ChunkInterestState& state : interests)
  {
    if (state.nodeId != nodeId) continue;
    state.position = position;
    state.interestArea = interestArea;
    return true;
  }

  if (interests.size() >= maxInterests) return false;
  interests.push_back({
      .nodeId = nodeId,
      .position = position,
      .interestArea = interestArea,
  });
  return true;
}

size_t ChunkInterestBucket::GetCount() const
{
  return interests.size();
}

const ChunkInterestState* ChunkInterestBucket::FindByNodeId(uint32_t nodeId) const
{
  for (const ChunkInterestState& state : interests)
  {
    if (state.nodeId == nodeId) return &state;
  }
  return nullptr;
}

std::vector<RuntimePeerAddress> ChunkInterestBucket::BuildInterestedPeerAddresses(
    const ActiveNodeBucket& activeNodes,
    uint32_t senderNodeId,
    const RuntimeWorldPosition& senderPosition,
    const RuntimeInterestArea& senderInterestArea) const
{
  std::vector<RuntimePeerAddress> peerAddresses = {};
  for (const ChunkInterestState& state : interests)
  {
    if (state.nodeId == senderNodeId) continue;
    if (!RuntimeInterestAreasOverlap(senderPosition, senderInterestArea, state.position, state.interestArea)) continue;
    const ActiveNodeState* activeNode = activeNodes.FindByNodeId(state.nodeId);
    if (activeNode == nullptr) continue;
    peerAddresses.push_back(activeNode->peerAddress);
  }
  return peerAddresses;
}
