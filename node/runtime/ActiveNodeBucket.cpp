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
    case ActiveNodeBucketCode::Added:            return "added";
    case ActiveNodeBucketCode::Refreshed:        return "refreshed";
    case ActiveNodeBucketCode::CapacityReached:  return "capacity_reached";
    case ActiveNodeBucketCode::UnknownPeer:      return "unknown_peer";
    case ActiveNodeBucketCode::InvalidSession:   return "invalid_session";
    case ActiveNodeBucketCode::InvalidSequence:  return "invalid_sequence";
    case ActiveNodeBucketCode::QuarantinedPeer:  return "quarantined_peer";
    case ActiveNodeBucketCode::BannedPeer:       return "banned_peer";
    default:                                     return "unknown";
  }
}

TopologyNodeState ToTopologyNodeState(const ActiveNodeState& node)
{
  return {
      .sessionId = node.sessionId,
      .protocolVersion = node.protocolVersion,
      .realmHash = node.realmHash,
      .peerAddress = node.peerAddress,
      .position = node.position,
      .interestArea = node.interestArea,
      .nodeRole = node.nodeRole,
      .acceptsJoins = (uint8_t)(node.acceptsJoins ? 1 : 0),
  };
}

uint32_t* ActiveNodeBucket::SelectSequenceSlot(ActiveNodeState* node, PacketType packetType)
{
  if (node == nullptr) return nullptr;

  switch (packetType)
  {
    case PacketType::Handshake:         return &node->lastHandshakeSequence;
    case PacketType::JoinRequest:       return &node->lastJoinRequestSequence;
    case PacketType::JoinResponse:      return &node->lastJoinResponseSequence;
    case PacketType::TopologySnapshot:  return &node->lastTopologySequence;
    case PacketType::PlayerSnapshot:    return &node->lastPlayerSequence;
    case PacketType::ChallengeRequest:  return &node->lastChallengeRequestSequence;
    case PacketType::ChallengeResponse: return &node->lastChallengeResponseSequence;
    default:                            return nullptr;
  }
}

const uint32_t* ActiveNodeBucket::SelectSequenceSlot(const ActiveNodeState& node, PacketType packetType)
{
  switch (packetType)
  {
    case PacketType::Handshake:         return &node.lastHandshakeSequence;
    case PacketType::JoinRequest:       return &node.lastJoinRequestSequence;
    case PacketType::JoinResponse:      return &node.lastJoinResponseSequence;
    case PacketType::TopologySnapshot:  return &node.lastTopologySequence;
    case PacketType::PlayerSnapshot:    return &node.lastPlayerSequence;
    case PacketType::ChallengeRequest:  return &node.lastChallengeRequestSequence;
    case PacketType::ChallengeResponse: return &node.lastChallengeResponseSequence;
    default:                            return nullptr;
  }
}

bool ActiveNodeBucket::IsNodeIsolated(const ActiveNodeState& node, uint64_t tick)
{
  return node.security.banned || (tick != UINT64_MAX && node.security.quarantinedUntilTick > tick);
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

    if (candidate.sessionId != 0 && node.sessionId != candidate.sessionId)
    {
      node.sessionId = candidate.sessionId;
      node.lastHandshakeSequence = 0;
      node.lastJoinRequestSequence = 0;
      node.lastJoinResponseSequence = 0;
      node.lastTopologySequence = 0;
      node.lastPlayerSequence = 0;
      node.lastChallengeRequestSequence = 0;
      node.lastChallengeResponseSequence = 0;
      node.security.rejectedPackets = 0;
      node.security.lastSecurityReason.clear();
      node.authenticated = false;
      node.pendingAuthChallengeNonce = 0;
      node.pendingAuthChallengeTick = 0;
    }

    node.protocolVersion = candidate.protocolVersion;
    node.realmHash = candidate.realmHash;
    node.position = candidate.position;
    node.interestArea = candidate.interestArea;
    node.nodeRole = candidate.nodeRole;
    node.acceptsJoins = candidate.acceptsJoins;
    node.lastPacketChecksum = candidate.lastPacketChecksum;
    node.lastSeenTick = tick;
    node.acceptedPackets += candidate.acceptedPackets > 0 ? candidate.acceptedPackets : 1;
    if (candidate.sessionId != 0) node.sessionId = candidate.sessionId;
    if (!candidate.signerAddress.empty()) node.signerAddress = candidate.signerAddress;
    if (candidate.advertisedChallengeNonce != 0) node.advertisedChallengeNonce = candidate.advertisedChallengeNonce;
    if (candidate.lastHandshakeSequence > 0) node.lastHandshakeSequence = candidate.lastHandshakeSequence;
    if (candidate.lastJoinRequestSequence > 0) node.lastJoinRequestSequence = candidate.lastJoinRequestSequence;
    if (candidate.lastJoinResponseSequence > 0) node.lastJoinResponseSequence = candidate.lastJoinResponseSequence;
    if (candidate.lastTopologySequence > 0) node.lastTopologySequence = candidate.lastTopologySequence;
    if (candidate.lastPlayerSequence > 0) node.lastPlayerSequence = candidate.lastPlayerSequence;
    if (candidate.lastChallengeRequestSequence > 0) node.lastChallengeRequestSequence = candidate.lastChallengeRequestSequence;
    if (candidate.lastChallengeResponseSequence > 0) node.lastChallengeResponseSequence = candidate.lastChallengeResponseSequence;
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
  ActiveNodeState* existing = FindMutableByPeerAddress(peerAddress);
  if (existing != nullptr)
  {
    if (existing->security.banned)
    {
      return {.accepted = false, .code = ActiveNodeBucketCode::BannedPeer, .node = *existing};
    }
    if (handshake.proof.sessionId == 0)
    {
      return {.accepted = false, .code = ActiveNodeBucketCode::InvalidSession, .node = *existing};
    }
    if (existing->sessionId == handshake.proof.sessionId && handshake.proof.sequence <= existing->lastHandshakeSequence)
    {
      return {.accepted = false, .code = ActiveNodeBucketCode::InvalidSequence, .node = *existing};
    }
  }

  ActiveNodeState node = {};
  node.peerAddress = peerAddress;
  node.sessionId = handshake.proof.sessionId;
  node.protocolVersion = handshake.protocolVersion;
  node.realmHash = handshake.realmHash;
  node.position = handshake.position;
  node.interestArea = handshake.interestArea;
  node.nodeRole = handshake.nodeRole;
  node.acceptsJoins = handshake.acceptsJoins != 0;
  node.lastPacketChecksum = packetChecksum;
  node.acceptedPackets = 1;
  node.lastHandshakeSequence = handshake.proof.sequence;
  node.advertisedChallengeNonce = handshake.challengeNonce;
  node.signerAddress = handshake.signerAddress;
  return UpsertNode(node, tick);
}

ActiveNodeBucketResult ActiveNodeBucket::RegisterTopologyNode(
    const TopologyNodeState& topologyNode,
    uint32_t packetChecksum,
    uint64_t tick)
{
  ActiveNodeState node = {};
  node.peerAddress = topologyNode.peerAddress;
  node.sessionId = topologyNode.sessionId;
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

ActiveNodeBucketResult ActiveNodeBucket::AcceptPacketMetadata(
    const RuntimePeerAddress& peerAddress,
    PacketType packetType,
    const PacketPeerProof& proof,
    uint32_t packetChecksum,
    uint64_t tick)
{
  ActiveNodeState* node = FindMutableByPeerAddress(peerAddress);
  if (node == nullptr)
  {
    return {.accepted = false, .code = ActiveNodeBucketCode::UnknownPeer};
  }
  if (node->security.banned)
  {
    return {.accepted = false, .code = ActiveNodeBucketCode::BannedPeer, .node = *node};
  }
  if (node->security.quarantinedUntilTick > tick)
  {
    return {.accepted = false, .code = ActiveNodeBucketCode::QuarantinedPeer, .node = *node};
  }
  if (proof.sessionId == 0 || node->sessionId == 0 || proof.sessionId != node->sessionId)
  {
    return {.accepted = false, .code = ActiveNodeBucketCode::InvalidSession, .node = *node};
  }

  uint32_t* sequenceSlot = SelectSequenceSlot(node, packetType);
  if (sequenceSlot == nullptr || proof.sequence == 0 || proof.sequence <= *sequenceSlot)
  {
    return {.accepted = false, .code = ActiveNodeBucketCode::InvalidSequence, .node = *node};
  }

  *sequenceSlot = proof.sequence;
  node->lastPacketChecksum = packetChecksum;
  node->lastSeenTick = tick;
  node->acceptedPackets += 1;
  return {.accepted = true, .code = ActiveNodeBucketCode::Refreshed, .node = *node};
}

ActiveNodeBucketResult ActiveNodeBucket::AddPeerStrike(
    const RuntimePeerAddress& peerAddress,
    const std::string& reason,
    uint32_t suspicionDelta,
    uint32_t strikeDelta,
    uint64_t tick,
    uint64_t quarantineDurationMs)
{
  ActiveNodeState* node = FindMutableByPeerAddress(peerAddress);
  if (node == nullptr)
  {
    return {.accepted = false, .code = ActiveNodeBucketCode::UnknownPeer};
  }

  node->security.rejectedPackets += 1;
  node->security.strikeCount += strikeDelta;
  node->security.suspicionScore += suspicionDelta;
  node->security.lastSecurityTick = tick;
  node->security.lastSecurityReason = reason;
  node->connected = false;
  node->authenticated = false;

  ActiveNodeBucketCode code = ActiveNodeBucketCode::Refreshed;
  if (node->security.banned)
  {
    code = ActiveNodeBucketCode::BannedPeer;
  }
  else if (node->security.strikeCount >= 6 || node->security.suspicionScore >= 10)
  {
    node->security.banned = true;
    node->security.quarantinedUntilTick = UINT64_MAX;
    code = ActiveNodeBucketCode::BannedPeer;
  }
  else if (node->security.strikeCount >= 3 || node->security.suspicionScore >= 5)
  {
    const uint64_t isolateUntil = tick + std::max<uint64_t>(quarantineDurationMs, 1);
    node->security.quarantinedUntilTick = std::max(node->security.quarantinedUntilTick, isolateUntil);
    code = ActiveNodeBucketCode::QuarantinedPeer;
  }

  return {.accepted = false, .code = code, .node = *node};
}

void ActiveNodeBucket::MarkConnected(const RuntimePeerAddress& peerAddress, bool connected)
{
  ActiveNodeState* node = FindMutableByPeerAddress(peerAddress);
  if (node == nullptr) return;
  node->connected = connected;
}

void ActiveNodeBucket::ForgetStaleNodes(uint64_t minimumTick, uint64_t nowTick)
{
  nodes.erase(
      std::remove_if(nodes.begin(), nodes.end(), [&](const ActiveNodeState& node)
                     {
                       if (node.security.banned) return false;
                       if (node.security.quarantinedUntilTick > nowTick) return false;
                       return node.lastSeenTick < minimumTick;
                     }),
      nodes.end());
}

void ActiveNodeBucket::ClearExpiredIsolation(uint64_t tick)
{
  for (ActiveNodeState& node : nodes)
  {
    if (!node.security.banned && node.security.quarantinedUntilTick > 0 && node.security.quarantinedUntilTick <= tick)
    {
      node.security.quarantinedUntilTick = 0;
    }
  }
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

bool ActiveNodeBucket::IsPeerIsolated(const RuntimePeerAddress& peerAddress, uint64_t tick) const
{
  const ActiveNodeState* node = FindByPeerAddress(peerAddress);
  return node != nullptr && IsNodeIsolated(*node, tick);
}

bool ActiveNodeBucket::IsPeerBanned(const RuntimePeerAddress& peerAddress) const
{
  const ActiveNodeState* node = FindByPeerAddress(peerAddress);
  return node != nullptr && node->security.banned;
}

bool ActiveNodeBucket::IsPeerAuthenticated(const RuntimePeerAddress& peerAddress) const
{
  const ActiveNodeState* node = FindByPeerAddress(peerAddress);
  return node != nullptr && node->authenticated && !node->signerAddress.empty();
}

std::vector<TopologyNodeState> ActiveNodeBucket::BuildTopologySnapshot(
    const RuntimePeerAddress& excludedPeerAddress,
    uint64_t realmHash,
    size_t maxCount,
    uint64_t tick) const
{
  std::vector<TopologyNodeState> snapshot = {};
  for (const ActiveNodeState& node : nodes)
  {
    if (RuntimePeerAddressEquals(node.peerAddress, excludedPeerAddress)) continue;
    if (node.realmHash != realmHash) continue;
    if (IsNodeIsolated(node, tick) || !node.authenticated) continue;
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
    bool requireJoinable,
    uint64_t tick) const
{
  std::vector<const ActiveNodeState*> ordered = {};
  for (const ActiveNodeState& node : nodes)
  {
    if (RuntimePeerAddressEquals(node.peerAddress, excludedPeerAddress)) continue;
    if (node.realmHash != realmHash) continue;
    if (requireJoinable && !node.acceptsJoins) continue;
    if (IsNodeIsolated(node, tick) || !node.authenticated) continue;
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
    size_t maxCount,
    uint64_t tick) const
{
  std::vector<const ActiveNodeState*> ordered = {};
  for (const ActiveNodeState& node : nodes)
  {
    if (RuntimePeerAddressEquals(node.peerAddress, excludedPeerAddress)) continue;
    if (node.realmHash != realmHash) continue;
    if (IsNodeIsolated(node, tick) || !node.authenticated) continue;
    if (!RuntimeInterestAreasOverlap(localPosition, localInterestArea, node.position, node.interestArea)) continue;
    ordered.push_back(&node);
  }

  std::sort(ordered.begin(), ordered.end(), [&](const ActiveNodeState* a, const ActiveNodeState* b)
            { return ComputeRuntimeWorldDistanceSquared(a->position, localPosition) < ComputeRuntimeWorldDistanceSquared(b->position, localPosition); });

  if (ordered.size() > maxCount) ordered.resize(maxCount);
  return ordered;
}
