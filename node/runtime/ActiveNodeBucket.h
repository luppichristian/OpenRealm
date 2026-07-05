#pragma once

#include "Packet.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ActiveNodeState
{
  uint32_t nodeId = 0;
  RuntimePeerAddress peerAddress = {};
  uint32_t protocolVersion = 0;
  uint64_t realmHash = 0;
  uint32_t lastPacketChecksum = 0;
  uint32_t acceptedPackets = 0;
};

enum class ActiveNodeBucketCode : uint8_t
{
  Added = 0,
  Refreshed = 1,
  DuplicateNodeId = 2,
  DuplicatePeerAddress = 3,
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
  ActiveNodeBucketResult RegisterHandshake(
      const RuntimePeerAddress& peerAddress,
      const HandshakePacketData& handshake,
      uint32_t packetChecksum
  );

  size_t GetCount() const;
  const ActiveNodeState* FindByNodeId(uint32_t nodeId) const;
  const ActiveNodeState* FindByPeerAddress(const RuntimePeerAddress& peerAddress) const;
  std::vector<RuntimePeerAddress> BuildPeerAddresses(uint32_t excludedNodeId, uint64_t realmHash) const;
  std::vector<PeerDiscoveryNodeState> BuildPeerDiscoveryNodes(uint32_t requestingNodeId, uint64_t realmHash) const;

 private:
  std::vector<ActiveNodeState> nodes = {};
};

bool RuntimePeerAddressEquals(const RuntimePeerAddress& a, const RuntimePeerAddress& b);
std::string DescribeRuntimePeerAddress(const RuntimePeerAddress& peerAddress);
std::string DescribeActiveNodeBucketCode(ActiveNodeBucketCode code);
