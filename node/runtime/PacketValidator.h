#pragma once

#include "ActiveNodeBucket.h"
#include "ProtocolVersion.h"

#include <cstdint>
#include <string>
#include <vector>

struct PacketValidationContext
{
  RuntimePeerAddress localPeerAddress = {};
  RuntimePeerAddress bootstrapPeerAddress = {};
  uint32_t expectedProtocolVersion = kRuntimeProtocolVersion;
  uint64_t expectedRealmHash = 0;
  ActiveNodeBucket* activeNodes = nullptr;
  uint64_t tick = 0;
  size_t maxKnownNodes = 64;
  uint32_t maxJoinCandidates = 6;
  uint32_t maxJoinHops = 8;
};

enum class PacketValidationCode : uint8_t
{
  Accepted = 0,
  PacketParseFailed = 1,
  InvalidHandshakePayload = 2,
  SelfPeerAddress = 3,
  ProtocolVersionMismatch = 4,
  RealmMismatch = 5,
  MissingActiveNodeBucket = 6,
  ActiveNodeLimitReached = 7,
  UnknownPeer = 8,
  InvalidSession = 9,
  InvalidSequence = 10,
  QuarantinedPeer = 11,
  BannedPeer = 12,
  InvalidJoinRequestPayload = 13,
  InvalidJoinResponsePayload = 14,
  InvalidTopologySnapshotPayload = 15,
  InvalidPlayerSnapshotPayload = 16,
  InvalidChallengeRequestPayload = 17,
  InvalidChallengeResponsePayload = 18,
  InvalidSignature = 19,
  MissingSignerAddress = 20,
  UnauthenticatedPeer = 21,
};

struct PacketValidationResult
{
  bool accepted = false;
  PacketValidationCode code = PacketValidationCode::PacketParseFailed;
  Packet packet = {};
  HandshakePacketData handshake = {};
  uint32_t packetChecksum = 0;
  ActiveNodeBucketResult activeNode = {};
};

PacketValidationResult ValidateIncomingPacket(
    const std::vector<uint8_t>& bytes,
    const RuntimePeerAddress& peerAddress,
    const PacketValidationContext& context);

std::string DescribePacketValidationCode(PacketValidationCode code);
