#include "RuntimePacket.h"

static constexpr uint32_t RUNTIME_PACKET_MAGIC = 0x4f524c4d;
static constexpr uint8_t RUNTIME_PACKET_VERSION = 1;

static void AppendU32(std::vector<uint8_t>* bytes, uint32_t value)
{
  if (bytes == nullptr) return;
  bytes->push_back((uint8_t)(value & 0xff));
  bytes->push_back((uint8_t)((value >> 8) & 0xff));
  bytes->push_back((uint8_t)((value >> 16) & 0xff));
  bytes->push_back((uint8_t)((value >> 24) & 0xff));
}

static uint16_t ReadU16(const std::vector<uint8_t>& bytes, size_t offset)
{
  return (uint16_t)(bytes[offset] | (bytes[offset + 1] << 8));
}

static uint32_t ReadU32(const std::vector<uint8_t>& bytes, size_t offset)
{
  return (uint32_t)bytes[offset]
       | ((uint32_t)bytes[offset + 1] << 8)
       | ((uint32_t)bytes[offset + 2] << 16)
       | ((uint32_t)bytes[offset + 3] << 24);
}

RuntimePacket MakeHandshakePacket(uint32_t protocolVersion, uint32_t nodeId)
{
  RuntimePacket packet = {};
  packet.type = RuntimePacketType::Handshake;
  AppendU32(&packet.payload, protocolVersion);
  AppendU32(&packet.payload, nodeId);
  return packet;
}

std::vector<uint8_t> SerializeRuntimePacket(const RuntimePacket& packet)
{
  RuntimePacketHeader header = {};
  header.magic = RUNTIME_PACKET_MAGIC;
  header.version = RUNTIME_PACKET_VERSION;
  header.type = (uint8_t)packet.type;
  header.payloadSize = (uint32_t)packet.payload.size();

  std::vector<uint8_t> bytes = {};
  bytes.reserve(12 + packet.payload.size());
  AppendU32(&bytes, header.magic);
  bytes.push_back(header.version);
  bytes.push_back(header.type);
  bytes.push_back((uint8_t)(header.reserved & 0xff));
  bytes.push_back((uint8_t)((header.reserved >> 8) & 0xff));
  AppendU32(&bytes, header.payloadSize);
  bytes.insert(bytes.end(), packet.payload.begin(), packet.payload.end());
  return bytes;
}

bool TryParseRuntimePacket(const std::vector<uint8_t>& bytes, RuntimePacket* packet)
{
  if (packet == nullptr) return false;
  if (bytes.size() < 12) return false;

  RuntimePacketHeader header = {};
  header.magic = ReadU32(bytes, 0);
  header.version = bytes[4];
  header.type = bytes[5];
  header.reserved = ReadU16(bytes, 6);
  header.payloadSize = ReadU32(bytes, 8);

  if (header.magic != RUNTIME_PACKET_MAGIC) return false;
  if (header.version != RUNTIME_PACKET_VERSION) return false;
  if (bytes.size() != (size_t)(12 + header.payloadSize)) return false;

  packet->type = (RuntimePacketType)header.type;
  packet->payload.assign(bytes.begin() + 12, bytes.end());
  return true;
}

std::string DescribeRuntimePacketType(RuntimePacketType type)
{
  switch (type)
  {
    case RuntimePacketType::Handshake: return "handshake";
    case RuntimePacketType::PeerDiscovery: return "peer_discovery";
    case RuntimePacketType::ChunkDelta: return "chunk_delta";
    case RuntimePacketType::WorldEvent: return "world_event";
    default: return "invalid";
  }
}
