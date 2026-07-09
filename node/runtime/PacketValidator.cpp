#include "PacketValidator.h"

std::string DescribePacketValidationCode(PacketValidationCode code)
{
  switch (code)
  {
    case PacketValidationCode::Accepted:                return "accepted";
    case PacketValidationCode::PacketParseFailed:       return "packet_parse_failed";
    case PacketValidationCode::InvalidHandshakePayload: return "invalid_handshake_payload";
    case PacketValidationCode::SelfPeerAddress:         return "self_peer_address";
    case PacketValidationCode::ProtocolVersionMismatch: return "protocol_version_mismatch";
    case PacketValidationCode::RealmMismatch:           return "realm_mismatch";
    case PacketValidationCode::DuplicatePeerAddress:    return "duplicate_peer_address";
    case PacketValidationCode::MissingActiveNodeBucket: return "missing_active_node_bucket";
    case PacketValidationCode::ActiveNodeLimitReached:  return "active_node_limit_reached";
    default:                                            return "unknown";
  }
}

PacketValidationResult ValidateIncomingPacket(
    const std::vector<uint8_t>& bytes,
    const RuntimePeerAddress& peerAddress,
    const PacketValidationContext& context)
{
  PacketValidationResult result = {};
  if (!TryParsePacket(bytes, &result.packet)) return result;

  result.packetChecksum = result.packet.checksum;
  if (result.packet.type != PacketType::Handshake)
  {
    result.accepted = true;
    result.code = PacketValidationCode::Accepted;
    return result;
  }

  if (!TryDecodeHandshakePacket(result.packet, &result.handshake))
  {
    result.code = PacketValidationCode::InvalidHandshakePayload;
    return result;
  }

  if (RuntimePeerAddressEquals(peerAddress, context.localPeerAddress))
  {
    result.code = PacketValidationCode::SelfPeerAddress;
    return result;
  }

  if (result.handshake.protocolVersion != context.expectedProtocolVersion)
  {
    result.code = PacketValidationCode::ProtocolVersionMismatch;
    return result;
  }

  if (result.handshake.realmHash != context.expectedRealmHash)
  {
    result.code = PacketValidationCode::RealmMismatch;
    return result;
  }

  if (context.activeNodes == nullptr)
  {
    result.code = PacketValidationCode::MissingActiveNodeBucket;
    return result;
  }

  result.activeNode = context.activeNodes->RegisterHandshake(
      peerAddress,
      result.handshake,
      result.packet.checksum,
      context.tick);
  if (!result.activeNode.accepted)
  {
    result.code = result.activeNode.code == ActiveNodeBucketCode::DuplicatePeerAddress
                      ? PacketValidationCode::DuplicatePeerAddress
                      : PacketValidationCode::ActiveNodeLimitReached;
    return result;
  }

  result.accepted = true;
  result.code = PacketValidationCode::Accepted;
  return result;
}
