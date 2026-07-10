#include "PacketValidator.h"

#include "RuntimeAuth.h"

#include <cmath>
#include <cstdio>

namespace
{
bool IsFinitePosition(const RuntimeWorldPosition& position)
{
  return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

bool IsFiniteInterestArea(const RuntimeInterestArea& area)
{
  return std::isfinite(area.radiusX) && std::isfinite(area.radiusY) && std::isfinite(area.radiusZ) && area.radiusX >= 0.0f && area.radiusY >= 0.0f && area.radiusZ >= 0.0f;
}

bool IsValidPeerAddress(const RuntimePeerAddress& peerAddress)
{
  return !peerAddress.host.empty() && peerAddress.port > 0 && peerAddress.port <= 65535;
}

PacketValidationCode FromBucketCode(ActiveNodeBucketCode code)
{
  switch (code)
  {
    case ActiveNodeBucketCode::CapacityReached: return PacketValidationCode::ActiveNodeLimitReached;
    case ActiveNodeBucketCode::UnknownPeer:     return PacketValidationCode::UnknownPeer;
    case ActiveNodeBucketCode::InvalidSession:  return PacketValidationCode::InvalidSession;
    case ActiveNodeBucketCode::InvalidSequence: return PacketValidationCode::InvalidSequence;
    case ActiveNodeBucketCode::QuarantinedPeer: return PacketValidationCode::QuarantinedPeer;
    case ActiveNodeBucketCode::BannedPeer:      return PacketValidationCode::BannedPeer;
    default:                                    return PacketValidationCode::Accepted;
  }
}

bool VerifyRecoveredAddress(const std::vector<uint8_t>& messageBytes, const std::string& signatureHex, const std::string& expectedAddress)
{
  if (signatureHex.empty() || expectedAddress.empty()) return false;
  std::string recoveredAddress = {};
  if (!RecoverRuntimeAuthSigner(messageBytes, signatureHex, &recoveredAddress)) return false;
  return NormalizeRuntimeAuthAddress(recoveredAddress) == NormalizeRuntimeAuthAddress(expectedAddress);
}

bool VerifyHandshakeSignature(const HandshakePacketData& handshake)
{
  if (handshake.signerAddress.empty() || handshake.proof.signatureHex.empty()) return false;
  std::vector<uint8_t> messageBytes = {};
  if (!BuildHandshakeSigningMessage(handshake, &messageBytes)) return false;
  std::string recoveredAddress = {};
  const bool recovered = RecoverRuntimeAuthSigner(messageBytes, handshake.proof.signatureHex, &recoveredAddress);
  const bool accepted = recovered && NormalizeRuntimeAuthAddress(recoveredAddress) == NormalizeRuntimeAuthAddress(handshake.signerAddress);
  if (!accepted)
  {
    std::fprintf(
        stderr,
        "[runtime-auth] handshake verify failed expected=%s recovered=%s recovered_ok=%d signature=%s\n",
        handshake.signerAddress.c_str(),
        recovered ? recoveredAddress.c_str() : "<recover_failed>",
        recovered ? 1 : 0,
        handshake.proof.signatureHex.c_str());
  }
  return accepted;
}

template<typename PacketData>
bool VerifySignedPacket(const PacketData& packetData, const std::string& expectedAddress, bool (*buildMessage)(const PacketData&, std::vector<uint8_t>*))
{
  if (expectedAddress.empty() || packetData.proof.signatureHex.empty()) return false;
  std::vector<uint8_t> messageBytes = {};
  if (!buildMessage(packetData, &messageBytes)) return false;
  return VerifyRecoveredAddress(messageBytes, packetData.proof.signatureHex, expectedAddress);
}
} // namespace

std::string DescribePacketValidationCode(PacketValidationCode code)
{
  switch (code)
  {
    case PacketValidationCode::Accepted:                       return "accepted";
    case PacketValidationCode::PacketParseFailed:              return "packet_parse_failed";
    case PacketValidationCode::InvalidHandshakePayload:        return "invalid_handshake_payload";
    case PacketValidationCode::SelfPeerAddress:                return "self_peer_address";
    case PacketValidationCode::ProtocolVersionMismatch:        return "protocol_version_mismatch";
    case PacketValidationCode::RealmMismatch:                  return "realm_mismatch";
    case PacketValidationCode::MissingActiveNodeBucket:        return "missing_active_node_bucket";
    case PacketValidationCode::ActiveNodeLimitReached:         return "active_node_limit_reached";
    case PacketValidationCode::UnknownPeer:                    return "unknown_peer";
    case PacketValidationCode::InvalidSession:                 return "invalid_session";
    case PacketValidationCode::InvalidSequence:                return "invalid_sequence";
    case PacketValidationCode::QuarantinedPeer:                return "quarantined_peer";
    case PacketValidationCode::BannedPeer:                     return "banned_peer";
    case PacketValidationCode::InvalidJoinRequestPayload:      return "invalid_join_request_payload";
    case PacketValidationCode::InvalidJoinResponsePayload:     return "invalid_join_response_payload";
    case PacketValidationCode::InvalidTopologySnapshotPayload: return "invalid_topology_snapshot_payload";
    case PacketValidationCode::InvalidPlayerSnapshotPayload:   return "invalid_player_snapshot_payload";
    case PacketValidationCode::InvalidChallengeRequestPayload: return "invalid_challenge_request_payload";
    case PacketValidationCode::InvalidChallengeResponsePayload:return "invalid_challenge_response_payload";
    case PacketValidationCode::InvalidSignature:               return "invalid_signature";
    case PacketValidationCode::MissingSignerAddress:           return "missing_signer_address";
    case PacketValidationCode::UnauthenticatedPeer:            return "unauthenticated_peer";
    default:                                                   return "unknown";
  }
}

PacketValidationResult ValidateIncomingPacket(
    const std::vector<uint8_t>& bytes,
    const RuntimePeerAddress& peerAddress,
    const PacketValidationContext& context)
{
  PacketValidationResult result = {};
  if (!TryParsePacket(bytes, &result.packet))
  {
    std::fprintf(
        stderr,
        "[runtime-auth] invalid packet parse peer=%s bytes=%d\n",
        DescribeRuntimePeerAddress(peerAddress).c_str(),
        (int)bytes.size());
    return result;
  }
  result.packetChecksum = result.packet.checksum;

  if (RuntimePeerAddressEquals(peerAddress, context.localPeerAddress))
  {
    result.code = PacketValidationCode::SelfPeerAddress;
    return result;
  }

  if (context.activeNodes == nullptr)
  {
    result.code = PacketValidationCode::MissingActiveNodeBucket;
    return result;
  }

  const ActiveNodeState* knownNode = context.activeNodes->FindByPeerAddress(peerAddress);

  switch (result.packet.type)
  {
    case PacketType::Handshake:
    {
      if (!TryDecodeHandshakePacket(result.packet, &result.handshake))
      {
        result.code = PacketValidationCode::InvalidHandshakePayload;
        return result;
      }
      if (result.handshake.proof.sessionId == 0 || result.handshake.proof.sequence == 0 || result.handshake.challengeNonce == 0
          || result.handshake.signerAddress.empty() || !IsFinitePosition(result.handshake.position) || !IsFiniteInterestArea(result.handshake.interestArea))
      {
        result.code = PacketValidationCode::InvalidHandshakePayload;
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
      if (!VerifyHandshakeSignature(result.handshake))
      {
        result.code = PacketValidationCode::InvalidSignature;
        return result;
      }

      result.activeNode = context.activeNodes->RegisterHandshake(peerAddress, result.handshake, result.packet.checksum, context.tick);
      if (!result.activeNode.accepted)
      {
        result.code = FromBucketCode(result.activeNode.code);
        if (result.code == PacketValidationCode::Accepted) result.code = PacketValidationCode::ActiveNodeLimitReached;
        return result;
      }
      break;
    }

    case PacketType::JoinRequest:
    {
      JoinRequestPacketData joinRequest = {};
      if (!TryDecodeJoinRequestPacket(result.packet, &joinRequest) || joinRequest.proof.sessionId == 0 || joinRequest.proof.sequence == 0 || joinRequest.proof.signatureHex.empty() || !IsFinitePosition(joinRequest.targetPosition) || joinRequest.maxCandidates == 0 || joinRequest.maxCandidates > context.maxJoinCandidates || joinRequest.maxHops == 0 || joinRequest.maxHops > context.maxJoinHops || joinRequest.requestToken == 0)
      {
        result.code = PacketValidationCode::InvalidJoinRequestPayload;
        return result;
      }
      if (knownNode == nullptr) { result.code = PacketValidationCode::UnknownPeer; return result; }
      if (!knownNode->authenticated) { result.code = PacketValidationCode::UnauthenticatedPeer; return result; }
      if (knownNode->signerAddress.empty()) { result.code = PacketValidationCode::MissingSignerAddress; return result; }
      if (!VerifySignedPacket(joinRequest, knownNode->signerAddress, BuildJoinRequestSigningMessage))
      {
        result.code = PacketValidationCode::InvalidSignature;
        return result;
      }
      result.activeNode = context.activeNodes->AcceptPacketMetadata(peerAddress, result.packet.type, joinRequest.proof, result.packet.checksum, context.tick);
      if (!result.activeNode.accepted)
      {
        result.code = FromBucketCode(result.activeNode.code);
        return result;
      }
      break;
    }

    case PacketType::JoinResponse:
    {
      JoinResponsePacketData joinResponse = {};
      if (!TryDecodeJoinResponsePacket(result.packet, &joinResponse, context.maxKnownNodes) || joinResponse.proof.sessionId == 0 || joinResponse.proof.sequence == 0 || joinResponse.proof.signatureHex.empty() || joinResponse.requestToken == 0 || !IsFinitePosition(joinResponse.resolvedPosition))
      {
        result.code = PacketValidationCode::InvalidJoinResponsePayload;
        return result;
      }
      if (knownNode == nullptr) { result.code = PacketValidationCode::UnknownPeer; return result; }
      if (!knownNode->authenticated) { result.code = PacketValidationCode::UnauthenticatedPeer; return result; }
      if (knownNode->signerAddress.empty()) { result.code = PacketValidationCode::MissingSignerAddress; return result; }
      if (!VerifySignedPacket(joinResponse, knownNode->signerAddress, BuildJoinResponseSigningMessage))
      {
        result.code = PacketValidationCode::InvalidSignature;
        return result;
      }
      result.activeNode = context.activeNodes->AcceptPacketMetadata(peerAddress, result.packet.type, joinResponse.proof, result.packet.checksum, context.tick);
      if (!result.activeNode.accepted)
      {
        result.code = FromBucketCode(result.activeNode.code);
        return result;
      }
      break;
    }

    case PacketType::TopologySnapshot:
    {
      TopologySnapshotPacketData topologySnapshot = {};
      if (!TryDecodeTopologySnapshotPacket(result.packet, &topologySnapshot, context.maxKnownNodes) || topologySnapshot.proof.sessionId == 0 || topologySnapshot.proof.sequence == 0 || topologySnapshot.proof.signatureHex.empty())
      {
        result.code = PacketValidationCode::InvalidTopologySnapshotPayload;
        return result;
      }
      for (const TopologyNodeState& node : topologySnapshot.nodes)
      {
        if (node.sessionId == 0 || node.protocolVersion != context.expectedProtocolVersion || node.realmHash != context.expectedRealmHash || !IsValidPeerAddress(node.peerAddress) || !IsFinitePosition(node.position) || !IsFiniteInterestArea(node.interestArea))
        {
          result.code = PacketValidationCode::InvalidTopologySnapshotPayload;
          return result;
        }
      }
      if (knownNode == nullptr) { result.code = PacketValidationCode::UnknownPeer; return result; }
      if (!knownNode->authenticated) { result.code = PacketValidationCode::UnauthenticatedPeer; return result; }
      if (knownNode->signerAddress.empty()) { result.code = PacketValidationCode::MissingSignerAddress; return result; }
      if (!VerifySignedPacket(topologySnapshot, knownNode->signerAddress, BuildTopologySnapshotSigningMessage))
      {
        result.code = PacketValidationCode::InvalidSignature;
        return result;
      }
      result.activeNode = context.activeNodes->AcceptPacketMetadata(peerAddress, result.packet.type, topologySnapshot.proof, result.packet.checksum, context.tick);
      if (!result.activeNode.accepted)
      {
        result.code = FromBucketCode(result.activeNode.code);
        return result;
      }
      break;
    }

    case PacketType::PlayerSnapshot:
    {
      PlayerSnapshotPacketData playerSnapshot = {};
      if (!TryDecodePlayerSnapshotPacket(result.packet, &playerSnapshot) || playerSnapshot.proof.sessionId == 0 || playerSnapshot.proof.sequence == 0 || playerSnapshot.proof.signatureHex.empty() || !IsFinitePosition(playerSnapshot.nodePosition) || !IsFinitePosition(playerSnapshot.playerPosition) || !std::isfinite(playerSnapshot.yaw) || !std::isfinite(playerSnapshot.pitch) || playerSnapshot.active > 1)
      {
        result.code = PacketValidationCode::InvalidPlayerSnapshotPayload;
        return result;
      }
      if (knownNode == nullptr) { result.code = PacketValidationCode::UnknownPeer; return result; }
      if (!knownNode->authenticated) { result.code = PacketValidationCode::UnauthenticatedPeer; return result; }
      if (knownNode->signerAddress.empty()) { result.code = PacketValidationCode::MissingSignerAddress; return result; }
      if (!VerifySignedPacket(playerSnapshot, knownNode->signerAddress, BuildPlayerSnapshotSigningMessage))
      {
        result.code = PacketValidationCode::InvalidSignature;
        return result;
      }
      result.activeNode = context.activeNodes->AcceptPacketMetadata(peerAddress, result.packet.type, playerSnapshot.proof, result.packet.checksum, context.tick);
      if (!result.activeNode.accepted)
      {
        result.code = FromBucketCode(result.activeNode.code);
        return result;
      }
      break;
    }

    case PacketType::ChallengeRequest:
    {
      ChallengeRequestPacketData challengeRequest = {};
      if (!TryDecodeChallengeRequestPacket(result.packet, &challengeRequest) || challengeRequest.proof.sessionId == 0 || challengeRequest.proof.sequence == 0 || challengeRequest.proof.signatureHex.empty() || challengeRequest.challengeNonce == 0)
      {
        result.code = PacketValidationCode::InvalidChallengeRequestPayload;
        return result;
      }
      if (knownNode == nullptr) { result.code = PacketValidationCode::UnknownPeer; return result; }
      if (knownNode->signerAddress.empty()) { result.code = PacketValidationCode::MissingSignerAddress; return result; }
      if (!VerifySignedPacket(challengeRequest, knownNode->signerAddress, BuildChallengeRequestSigningMessage))
      {
        result.code = PacketValidationCode::InvalidSignature;
        return result;
      }
      result.activeNode = context.activeNodes->AcceptPacketMetadata(peerAddress, result.packet.type, challengeRequest.proof, result.packet.checksum, context.tick);
      if (!result.activeNode.accepted)
      {
        result.code = FromBucketCode(result.activeNode.code);
        return result;
      }
      break;
    }

    case PacketType::ChallengeResponse:
    {
      ChallengeResponsePacketData challengeResponse = {};
      if (!TryDecodeChallengeResponsePacket(result.packet, &challengeResponse) || challengeResponse.proof.sessionId == 0 || challengeResponse.proof.sequence == 0 || challengeResponse.proof.signatureHex.empty() || challengeResponse.challengeNonce == 0)
      {
        result.code = PacketValidationCode::InvalidChallengeResponsePayload;
        return result;
      }
      if (knownNode == nullptr) { result.code = PacketValidationCode::UnknownPeer; return result; }
      if (knownNode->signerAddress.empty()) { result.code = PacketValidationCode::MissingSignerAddress; return result; }
      if (!VerifySignedPacket(challengeResponse, knownNode->signerAddress, BuildChallengeResponseSigningMessage))
      {
        result.code = PacketValidationCode::InvalidSignature;
        return result;
      }
      result.activeNode = context.activeNodes->AcceptPacketMetadata(peerAddress, result.packet.type, challengeResponse.proof, result.packet.checksum, context.tick);
      if (!result.activeNode.accepted)
      {
        result.code = FromBucketCode(result.activeNode.code);
        return result;
      }
      break;
    }

    default:
      result.code = PacketValidationCode::PacketParseFailed;
      return result;
  }

  result.accepted = true;
  result.code = PacketValidationCode::Accepted;
  return result;
}
