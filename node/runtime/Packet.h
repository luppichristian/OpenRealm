#pragma once

#include "RuntimeClient.h"
#include "RuntimeWorldPosition.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class PacketType : uint8_t
{
  Invalid = 0,
  Handshake = 1,
  JoinRequest = 2,
  JoinResponse = 3,
  TopologySnapshot = 4,
  PlayerSnapshot = 5,
};

struct PacketHeader
{
  uint32_t magic = 0;
  uint8_t version = 1;
  uint8_t type = 0;
  uint16_t reserved = 0;
  uint32_t payloadSize = 0;
  uint32_t checksum = 0;
};

struct Packet
{
  PacketType type = PacketType::Invalid;
  uint32_t checksum = 0;
  std::vector<uint8_t> payload = {};
};

struct HandshakePacketData
{
  uint32_t protocolVersion = 0;
  uint64_t realmHash = 0;
  RuntimeWorldPosition position = {};
  RuntimeInterestArea interestArea = {};
  uint8_t nodeRole = 0;
  uint8_t acceptsJoins = 0;
  uint16_t reserved = 0;
};

struct TopologyNodeState
{
  uint32_t protocolVersion = 0;
  uint64_t realmHash = 0;
  RuntimePeerAddress peerAddress = {};
  RuntimeWorldPosition position = {};
  RuntimeInterestArea interestArea = {};
  uint8_t nodeRole = 0;
  uint8_t acceptsJoins = 0;
};

struct JoinRequestPacketData
{
  RuntimeWorldPosition targetPosition = {};
  uint32_t maxCandidates = 0;
  uint32_t maxHops = 0;
  uint32_t requestToken = 0;
};

struct JoinResponsePacketData
{
  uint32_t requestToken = 0;
  RuntimeWorldPosition resolvedPosition = {};
  std::vector<TopologyNodeState> candidates = {};
};

struct TopologySnapshotPacketData
{
  std::vector<TopologyNodeState> nodes = {};
};

struct PlayerSnapshotPacketData
{
  RuntimeWorldPosition nodePosition = {};
  RuntimeWorldPosition playerPosition = {};
  float yaw = 0.0f;
  float pitch = 0.0f;
  uint8_t active = 0;
};

Packet MakeHandshakePacket(const HandshakePacketData& handshake);
bool TryDecodeHandshakePacket(const Packet& packet, HandshakePacketData* handshake);
Packet MakeJoinRequestPacket(const JoinRequestPacketData& joinRequest);
bool TryDecodeJoinRequestPacket(const Packet& packet, JoinRequestPacketData* joinRequest);
Packet MakeJoinResponsePacket(const JoinResponsePacketData& joinResponse);
bool TryDecodeJoinResponsePacket(const Packet& packet, JoinResponsePacketData* joinResponse, size_t maxCandidates = 64);
Packet MakeTopologySnapshotPacket(const TopologySnapshotPacketData& topologySnapshot);
bool TryDecodeTopologySnapshotPacket(const Packet& packet, TopologySnapshotPacketData* topologySnapshot, size_t maxNodes = 128);
Packet MakePlayerSnapshotPacket(const PlayerSnapshotPacketData& playerSnapshot);
bool TryDecodePlayerSnapshotPacket(const Packet& packet, PlayerSnapshotPacketData* playerSnapshot);

std::vector<uint8_t> SerializePacket(const Packet& packet);
bool TryParsePacket(const std::vector<uint8_t>& bytes, Packet* packet);
uint32_t ComputePacketChecksum(const PacketHeader& header, const std::vector<uint8_t>& payload);
bool IsPacketTypeSupported(PacketType type);
std::string DescribePacketType(PacketType type);
