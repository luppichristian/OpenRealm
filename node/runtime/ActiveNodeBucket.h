#pragma once

#include "Packet.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ActiveNodeSecurityState
{
  uint32_t strikeCount = 0;
  uint32_t suspicionScore = 0;
  uint32_t rejectedPackets = 0;
  uint64_t quarantinedUntilTick = 0;
  uint64_t lastSecurityTick = 0;
  bool banned = false;
  std::string lastSecurityReason = {};
};

struct ActiveNodeState
{
  RuntimePeerAddress peerAddress = {};
  uint64_t sessionId = 0;
  uint32_t protocolVersion = 0;
  uint64_t realmHash = 0;
  RuntimeWorldPosition position = {};
  RuntimeInterestArea interestArea = {};
  uint8_t nodeRole = 0;
  bool acceptsJoins = false;
  bool connected = false;
  uint32_t lastPacketChecksum = 0;
  uint32_t acceptedPackets = 0;
  uint64_t lastSeenTick = 0;
  uint32_t lastHandshakeSequence = 0;
  uint32_t lastJoinRequestSequence = 0;
  uint32_t lastJoinResponseSequence = 0;
  uint32_t lastTopologySequence = 0;
  uint32_t lastPlayerSequence = 0;
  ActiveNodeSecurityState security = {};
};

enum class ActiveNodeBucketCode : uint8_t
{
  Added = 0,
  Refreshed = 1,
  CapacityReached = 2,
  UnknownPeer = 3,
  InvalidSession = 4,
  InvalidSequence = 5,
  QuarantinedPeer = 6,
  BannedPeer = 7,
};

struct ActiveNodeBucketResult
{
  bool accepted = false;
  ActiveNodeBucketCode code = ActiveNodeBucketCode::Added;
  ActiveNodeState node = {};
};

class ActiveNodeBucket
{
 public:
  explicit ActiveNodeBucket(size_t maxNodes = 32)
      : maxNodes(maxNodes)
  {
  }

  ActiveNodeBucketResult RegisterHandshake(
      const RuntimePeerAddress& peerAddress,
      const HandshakePacketData& handshake,
      uint32_t packetChecksum,
      uint64_t tick);

  ActiveNodeBucketResult RegisterTopologyNode(
      const TopologyNodeState& topologyNode,
      uint32_t packetChecksum,
      uint64_t tick);

  ActiveNodeBucketResult AcceptPacketMetadata(
      const RuntimePeerAddress& peerAddress,
      PacketType packetType,
      const PacketPeerProof& proof,
      uint32_t packetChecksum,
      uint64_t tick);

  ActiveNodeBucketResult AddPeerStrike(
      const RuntimePeerAddress& peerAddress,
      const std::string& reason,
      uint32_t suspicionDelta,
      uint32_t strikeDelta,
      uint64_t tick,
      uint64_t quarantineDurationMs);

  void MarkConnected(const RuntimePeerAddress& peerAddress, bool connected);
  void ForgetStaleNodes(uint64_t minimumTick, uint64_t nowTick);
  void ClearExpiredIsolation(uint64_t tick);

  size_t GetCount() const;
  const std::vector<ActiveNodeState>& GetNodes() const;
  const ActiveNodeState* FindByPeerAddress(const RuntimePeerAddress& peerAddress) const;
  ActiveNodeState* FindMutableByPeerAddress(const RuntimePeerAddress& peerAddress);
  bool IsPeerIsolated(const RuntimePeerAddress& peerAddress, uint64_t tick) const;
  bool IsPeerBanned(const RuntimePeerAddress& peerAddress) const;
  std::vector<TopologyNodeState> BuildTopologySnapshot(const RuntimePeerAddress& excludedPeerAddress, uint64_t realmHash, size_t maxCount = SIZE_MAX, uint64_t tick = UINT64_MAX) const;
  std::vector<const ActiveNodeState*> BuildClosestNodes(
      const RuntimePeerAddress& excludedPeerAddress,
      uint64_t realmHash,
      const RuntimeWorldPosition& targetPosition,
      size_t maxCount,
      bool requireJoinable,
      uint64_t tick = UINT64_MAX) const;
  std::vector<const ActiveNodeState*> BuildNeighborCandidates(
      const RuntimePeerAddress& excludedPeerAddress,
      uint64_t realmHash,
      const RuntimeWorldPosition& localPosition,
      const RuntimeInterestArea& localInterestArea,
      size_t maxCount,
      uint64_t tick = UINT64_MAX) const;

 private:
  ActiveNodeBucketResult UpsertNode(const ActiveNodeState& candidate, uint64_t tick);
  static uint32_t* SelectSequenceSlot(ActiveNodeState* node, PacketType packetType);
  static const uint32_t* SelectSequenceSlot(const ActiveNodeState& node, PacketType packetType);
  static bool IsNodeIsolated(const ActiveNodeState& node, uint64_t tick);

  size_t maxNodes = 32;
  std::vector<ActiveNodeState> nodes = {};
};

bool RuntimePeerAddressEquals(const RuntimePeerAddress& a, const RuntimePeerAddress& b);
std::string DescribeRuntimePeerAddress(const RuntimePeerAddress& peerAddress);
std::string DescribeActiveNodeBucketCode(ActiveNodeBucketCode code);
TopologyNodeState ToTopologyNodeState(const ActiveNodeState& node);
