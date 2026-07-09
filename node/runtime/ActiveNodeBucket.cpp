#include "ActiveNodeBucket.h"

#include <algorithm>

bool RuntimePeerAddressEquals(const RuntimePeerAddress& a, const RuntimePeerAddress& b)
{
  return a.host == b.host && a.port == b.port;
}

std::string DescribeRuntimePeerAddress(const RuntimePeerAddress& peerAddress)
{
  return peerAddress.host + ":" + std::to_string(peerAddress.port);
}

std::string DescribeActiveNodeBucketCode(ActiveNodeBucketCode code)
{
  switch (code)
  {
    case ActiveNodeBucketCode::Added:                return "added";
    case ActiveNodeBucketCode::Refreshed:            return "refreshed";
    case ActiveNodeBucketCode::DuplicatePeerAddress: return "duplicate_peer_address";
    case ActiveNodeBucketCode::CapacityReached:      return "capacity_reached";
    default:                                         return "unknown";
  }
}

TopologyNodeState ToTopologyNodeState(const ActiveNodeState& node)
{
  return {
      .protocolVersion = node.protocolVersion,
      .realmHash = node.realmHash,
      .peerAddress = node.peerAddress,
      .position = node.position,
      .interestArea = node.interestArea,
      .nodeRole = node.nodeRole,
      .acceptsJoins = (uint8_t)(node.acceptsJoins ? 1 : 0),
  };
}

ActiveNodeBucketResult ActiveNodeBucket::UpsertNode(const ActiveNodeState& candidate, uint64_t tick)
{
  ActiveNodeBucketResult result = {};
  result.node = candidate;
  result.node.lastSeenTick = tick;
  result.node.acceptedPackets = std::max(1u, result.node.acceptedPackets);

  for (ActiveNodeState& node : nodes)
  {
    if (!RuntimePeerAddressEquals(node.peerAddress, candidate.peerAddress)) continue;

    node.protocolVersion = candidate.protocolVersion;
    node.realmHash = candidate.realmHash;
    node.position = candidate.position;
    node.interestArea = candidate.interestArea;
    node.nodeRole = candidate.nodeRole;
    node.acceptsJoins = candidate.acceptsJoins;
    node.lastPacketChecksum = candidate.lastPacketChecksum;
    node.lastSeenTick = tick;
    node.acceptedPackets += candidate.acceptedPackets > 0 ? candidate.acceptedPackets : 1;
    result.code = ActiveNodeBucketCode::Refreshed;
    result.accepted = true;
    result.node = node;
    return result;
  }

  if (nodes.size() >= maxNodes)
  {
    result.code = ActiveNodeBucketCode::CapacityReached;
    result.accepted = false;
    return result;
  }

  nodes.push_back(result.node);
  result.code = ActiveNodeBucketCode::Added;
  result.accepted = true;
  return result;
}

ActiveNodeBucketResult ActiveNodeBucket::RegisterHandshake(
    const RuntimePeerAddress& peerAddress,
    const HandshakePacketData& handshake,
    uint32_t packetChecksum,
    uint64_t tick)
{
  ActiveNodeState node = {};
  node.peerAddress = peerAddress;
  node.protocolVersion = handshake.protocolVersion;
  node.realmHash = handshake.realmHash;
  node.position = handshake.position;
  node.interestArea = handshake.interestArea;
  node.nodeRole = handshake.nodeRole;
  node.acceptsJoins = handshake.acceptsJoins != 0;
  node.lastPacketChecksum = packetChecksum;
  node.acceptedPackets = 1;
  return UpsertNode(node, tick);
}

ActiveNodeBucketResult ActiveNodeBucket::RegisterTopologyNode(
    const TopologyNodeState& topologyNode,
    uint32_t packetChecksum,
    uint64_t tick)
{
  ActiveNodeState node = {};
  node.peerAddress = topologyNode.peerAddress;
  node.protocolVersion = topologyNode.protocolVersion;
  node.realmHash = topologyNode.realmHash;
  node.position = topologyNode.position;
  node.interestArea = topologyNode.interestArea;
  node.nodeRole = topologyNode.nodeRole;
  node.acceptsJoins = topologyNode.acceptsJoins != 0;
  node.lastPacketChecksum = packetChecksum;
  node.acceptedPackets = 1;
  return UpsertNode(node, tick);
}

void ActiveNodeBucket::MarkConnected(const RuntimePeerAddress& peerAddress, bool connected)
{
  ActiveNodeState* node = FindMutableByPeerAddress(peerAddress);
  if (node == nullptr) return;
  node->connected = connected;
}

void ActiveNodeBucket::ForgetStaleNodes(uint64_t minimumTick)
{
  nodes.erase(
      std::remove_if(nodes.begin(), nodes.end(), [&](const ActiveNodeState& node)
                     { return node.lastSeenTick < minimumTick; }),
      nodes.end());
}

size_t ActiveNodeBucket::GetCount() const
{
  return nodes.size();
}

const std::vector<ActiveNodeState>& ActiveNodeBucket::GetNodes() const
{
  return nodes;
}

const ActiveNodeState* ActiveNodeBucket::FindByPeerAddress(const RuntimePeerAddress& peerAddress) const
{
  for (const ActiveNodeState& node : nodes)
  {
    if (RuntimePeerAddressEquals(node.peerAddress, peerAddress)) return &node;
  }
  return nullptr;
}

ActiveNodeState* ActiveNodeBucket::FindMutableByPeerAddress(const RuntimePeerAddress& peerAddress)
{
  for (ActiveNodeState& node : nodes)
  {
    if (RuntimePeerAddressEquals(node.peerAddress, peerAddress)) return &node;
  }
  return nullptr;
}

std::vector<TopologyNodeState> ActiveNodeBucket::BuildTopologySnapshot(
    const RuntimePeerAddress& excludedPeerAddress,
    uint64_t realmHash,
    size_t maxCount) const
{
  std::vector<TopologyNodeState> snapshot = {};
  for (const ActiveNodeState& node : nodes)
  {
    if (RuntimePeerAddressEquals(node.peerAddress, excludedPeerAddress)) continue;
    if (node.realmHash != realmHash) continue;
    snapshot.push_back(ToTopologyNodeState(node));
    if (snapshot.size() >= maxCount) break;
  }
  return snapshot;
}

std::vector<const ActiveNodeState*> ActiveNodeBucket::BuildClosestNodes(
    const RuntimePeerAddress& excludedPeerAddress,
    uint64_t realmHash,
    const RuntimeWorldPosition& targetPosition,
    size_t maxCount,
    bool requireJoinable) const
{
  std::vector<const ActiveNodeState*> ordered = {};
  for (const ActiveNodeState& node : nodes)
  {
    if (RuntimePeerAddressEquals(node.peerAddress, excludedPeerAddress)) continue;
    if (node.realmHash != realmHash) continue;
    if (requireJoinable && !node.acceptsJoins) continue;
    ordered.push_back(&node);
  }

  std::sort(ordered.begin(), ordered.end(), [&](const ActiveNodeState* a, const ActiveNodeState* b)
            { return ComputeRuntimeWorldDistanceSquared(a->position, targetPosition) < ComputeRuntimeWorldDistanceSquared(b->position, targetPosition); });

  if (ordered.size() > maxCount) ordered.resize(maxCount);
  return ordered;
}

std::vector<const ActiveNodeState*> ActiveNodeBucket::BuildNeighborCandidates(
    const RuntimePeerAddress& excludedPeerAddress,
    uint64_t realmHash,
    const RuntimeWorldPosition& localPosition,
    const RuntimeInterestArea& localInterestArea,
    size_t maxCount) const
{
  std::vector<const ActiveNodeState*> ordered = {};
  for (const ActiveNodeState& node : nodes)
  {
    if (RuntimePeerAddressEquals(node.peerAddress, excludedPeerAddress)) continue;
    if (node.realmHash != realmHash) continue;
    if (!RuntimeInterestAreasOverlap(localPosition, localInterestArea, node.position, node.interestArea)) continue;
    ordered.push_back(&node);
  }

  std::sort(ordered.begin(), ordered.end(), [&](const ActiveNodeState* a, const ActiveNodeState* b)
            { return ComputeRuntimeWorldDistanceSquared(a->position, localPosition) < ComputeRuntimeWorldDistanceSquared(b->position, localPosition); });

  if (ordered.size() > maxCount) ordered.resize(maxCount);
  return ordered;
}
