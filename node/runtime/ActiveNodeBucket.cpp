#include "ActiveNodeBucket.h"

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
    case ActiveNodeBucketCode::Added: return "added";
    case ActiveNodeBucketCode::Refreshed: return "refreshed";
    case ActiveNodeBucketCode::DuplicateNodeId: return "duplicate_node_id";
    case ActiveNodeBucketCode::DuplicatePeerAddress: return "duplicate_peer_address";
    default: return "unknown";
  }
}

ActiveNodeBucketResult ActiveNodeBucket::RegisterHandshake(
    const RuntimePeerAddress& peerAddress,
    const HandshakePacketData& handshake,
    uint32_t packetChecksum
)
{
  ActiveNodeBucketResult result = {};
  result.node.nodeId = handshake.nodeId;
  result.node.peerAddress = peerAddress;
  result.node.protocolVersion = handshake.protocolVersion;
  result.node.realmHash = handshake.realmHash;
  result.node.lastPacketChecksum = packetChecksum;
  result.node.acceptedPackets = 1;

  for (ActiveNodeState& node : nodes)
  {
    if (node.nodeId == handshake.nodeId)
    {
      if (!RuntimePeerAddressEquals(node.peerAddress, peerAddress))
      {
        result.code = ActiveNodeBucketCode::DuplicateNodeId;
        result.accepted = false;
        result.node = node;
        return result;
      }

      node.protocolVersion = handshake.protocolVersion;
      node.realmHash = handshake.realmHash;
      node.lastPacketChecksum = packetChecksum;
      node.acceptedPackets += 1;
      result.code = ActiveNodeBucketCode::Refreshed;
      result.accepted = true;
      result.node = node;
      return result;
    }

    if (RuntimePeerAddressEquals(node.peerAddress, peerAddress) && node.nodeId != handshake.nodeId)
    {
      result.code = ActiveNodeBucketCode::DuplicatePeerAddress;
      result.accepted = false;
      result.node = node;
      return result;
    }
  }

  nodes.push_back(result.node);
  result.code = ActiveNodeBucketCode::Added;
  result.accepted = true;
  return result;
}

size_t ActiveNodeBucket::GetCount() const
{
  return nodes.size();
}

const ActiveNodeState* ActiveNodeBucket::FindByNodeId(uint32_t nodeId) const
{
  for (const ActiveNodeState& node : nodes)
  {
    if (node.nodeId == nodeId) return &node;
  }
  return nullptr;
}

const ActiveNodeState* ActiveNodeBucket::FindByPeerAddress(const RuntimePeerAddress& peerAddress) const
{
  for (const ActiveNodeState& node : nodes)
  {
    if (RuntimePeerAddressEquals(node.peerAddress, peerAddress)) return &node;
  }
  return nullptr;
}

std::vector<PeerDiscoveryNodeState> ActiveNodeBucket::BuildPeerDiscoveryNodes(
    uint32_t requestingNodeId,
    uint64_t realmHash
) const
{
  std::vector<PeerDiscoveryNodeState> discoveredNodes = {};

  for (const ActiveNodeState& node : nodes)
  {
    if (node.nodeId == requestingNodeId) continue;
    if (node.realmHash != realmHash) continue;

    PeerDiscoveryNodeState discoveredNode = {};
    discoveredNode.nodeId = node.nodeId;
    discoveredNode.protocolVersion = node.protocolVersion;
    discoveredNode.realmHash = node.realmHash;
    discoveredNode.peerAddress = node.peerAddress;
    discoveredNodes.push_back(discoveredNode);
  }

  return discoveredNodes;
}
