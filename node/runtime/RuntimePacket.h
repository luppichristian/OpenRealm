#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class RuntimePacketType : uint8_t
{
  Invalid = 0,
  Handshake = 1,
  PeerDiscovery = 2,
  ChunkDelta = 3,
  WorldEvent = 4,
};

struct RuntimePacketHeader
{
  uint32_t magic = 0;
  uint8_t version = 1;
  uint8_t type = 0;
  uint16_t reserved = 0;
  uint32_t payloadSize = 0;
};

struct RuntimePacket
{
  RuntimePacketType type = RuntimePacketType::Invalid;
  std::vector<uint8_t> payload = {};
};

RuntimePacket MakeHandshakePacket(uint32_t protocolVersion, uint32_t nodeId);
std::vector<uint8_t> SerializeRuntimePacket(const RuntimePacket& packet);
bool TryParseRuntimePacket(const std::vector<uint8_t>& bytes, RuntimePacket* packet);
std::string DescribeRuntimePacketType(RuntimePacketType type);
