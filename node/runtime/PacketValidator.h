#pragma once

#include "ActiveNodeBucket.h"

#include <cstdint>
#include <string>
#include <vector>

struct PacketValidationContext
{
  uint32_t localNodeId = 0;
  uint64_t expectedRealmHash = 0;
  ActiveNodeBucket* activeNodes = nullptr;
};

enum class PacketValidationCode : uint8_t
{
  Accepted = 0,
  PacketParseFailed = 1,
  InvalidHandshakePayload = 2,
  SelfNodeId = 3,
  RealmMismatch = 4,
  DuplicateNodeId = 5,
  DuplicatePeerAddress = 6,
  MissingActiveNodeBucket = 7,
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
    const PacketValidationContext& context
);

std::string DescribePacketValidationCode(PacketValidationCode code);
