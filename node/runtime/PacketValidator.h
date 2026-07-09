#pragma once

#include "ActiveNodeBucket.h"
#include "ProtocolVersion.h"

#include <cstdint>
#include <string>
#include <vector>

struct PacketValidationContext
{
  RuntimePeerAddress localPeerAddress = {};
  uint32_t expectedProtocolVersion = kRuntimeProtocolVersion;
  uint64_t expectedRealmHash = 0;
  ActiveNodeBucket* activeNodes = nullptr;
  uint64_t tick = 0;
};

enum class PacketValidationCode : uint8_t
{
  Accepted = 0,
  PacketParseFailed = 1,
  InvalidHandshakePayload = 2,
  SelfPeerAddress = 3,
  ProtocolVersionMismatch = 4,
  RealmMismatch = 5,
  DuplicatePeerAddress = 6,
  MissingActiveNodeBucket = 7,
  ActiveNodeLimitReached = 8,
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
