#pragma once

#include "RuntimeClient.h"

#include <cstdint>
#include <string>
#include <vector>

enum class PacketType : uint8_t
{
  Invalid = 0,
  Handshake = 1,
  PeerDiscovery = 2,
  ChunkDelta = 3,
  WorldEvent = 4,
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
  uint32_t nodeId = 0;
  uint64_t realmHash = 0;
};

struct PeerDiscoveryNodeState
{
  uint32_t nodeId = 0;
  uint32_t protocolVersion = 0;
  uint64_t realmHash = 0;
  RuntimePeerAddress peerAddress = {};
};

struct PeerDiscoveryPacketData
{
  uint32_t requestingNodeId = 0;
  std::vector<PeerDiscoveryNodeState> nodes = {};
};

Packet MakeHandshakePacket(const HandshakePacketData& handshake);
bool TryDecodeHandshakePacket(const Packet& packet, HandshakePacketData* handshake);
Packet MakePeerDiscoveryPacket(const PeerDiscoveryPacketData& peerDiscovery);
bool TryDecodePeerDiscoveryPacket(const Packet& packet, PeerDiscoveryPacketData* peerDiscovery);
std::vector<uint8_t> SerializePacket(const Packet& packet);
bool TryParsePacket(const std::vector<uint8_t>& bytes, Packet* packet);
uint32_t ComputePacketChecksum(const PacketHeader& header, const std::vector<uint8_t>& payload);
bool IsPacketTypeSupported(PacketType type);
std::string DescribePacketType(PacketType type);
