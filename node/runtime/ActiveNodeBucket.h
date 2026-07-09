#pragma once

#include "Packet.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ActiveNodeState
{
  RuntimePeerAddress peerAddress = {};
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
};

enum class ActiveNodeBucketCode : uint8_t
{
  Added = 0,
  Refreshed = 1,
  DuplicatePeerAddress = 2,
  CapacityReached = 3,
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

  void MarkConnected(const RuntimePeerAddress& peerAddress, bool connected);
  void ForgetStaleNodes(uint64_t minimumTick);

  size_t GetCount() const;
  const std::vector<ActiveNodeState>& GetNodes() const;
  const ActiveNodeState* FindByPeerAddress(const RuntimePeerAddress& peerAddress) const;
  ActiveNodeState* FindMutableByPeerAddress(const RuntimePeerAddress& peerAddress);
  std::vector<TopologyNodeState> BuildTopologySnapshot(const RuntimePeerAddress& excludedPeerAddress, uint64_t realmHash, size_t maxCount = SIZE_MAX) const;
  std::vector<const ActiveNodeState*> BuildClosestNodes(
      const RuntimePeerAddress& excludedPeerAddress,
      uint64_t realmHash,
      const RuntimeWorldPosition& targetPosition,
      size_t maxCount,
      bool requireJoinable) const;
  std::vector<const ActiveNodeState*> BuildNeighborCandidates(
      const RuntimePeerAddress& excludedPeerAddress,
      uint64_t realmHash,
      const RuntimeWorldPosition& localPosition,
      const RuntimeInterestArea& localInterestArea,
      size_t maxCount) const;

 private:
  ActiveNodeBucketResult UpsertNode(const ActiveNodeState& candidate, uint64_t tick);

  size_t maxNodes = 32;
  std::vector<ActiveNodeState> nodes = {};
};

bool RuntimePeerAddressEquals(const RuntimePeerAddress& a, const RuntimePeerAddress& b);
std::string DescribeRuntimePeerAddress(const RuntimePeerAddress& peerAddress);
std::string DescribeActiveNodeBucketCode(ActiveNodeBucketCode code);
TopologyNodeState ToTopologyNodeState(const ActiveNodeState& node);
