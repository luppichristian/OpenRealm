#include "Packet.h"

#include <enet/enet.h>

static constexpr uint32_t RUNTIME_PACKET_MAGIC = 0x4f524c4d;
static constexpr uint8_t RUNTIME_PACKET_VERSION = 1;
static constexpr size_t PACKET_HEADER_SIZE = 16;
static constexpr size_t HANDSHAKE_PAYLOAD_SIZE = 16;

static void AppendU32(std::vector<uint8_t>* bytes, uint32_t value)
{
  if (bytes == nullptr) return;
  bytes->push_back((uint8_t)(value & 0xff));
  bytes->push_back((uint8_t)((value >> 8) & 0xff));
  bytes->push_back((uint8_t)((value >> 16) & 0xff));
  bytes->push_back((uint8_t)((value >> 24) & 0xff));
}

static void AppendU64(std::vector<uint8_t>* bytes, uint64_t value)
{
  if (bytes == nullptr) return;
  for (int shift = 0; shift < 64; shift += 8)
  {
    bytes->push_back((uint8_t)((value >> shift) & 0xff));
  }
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

static uint64_t ReadU64(const std::vector<uint8_t>& bytes, size_t offset)
{
  uint64_t value = 0;
  for (int shift = 0; shift < 64; shift += 8)
  {
    value |= ((uint64_t)bytes[offset + (shift / 8)]) << shift;
  }
  return value;
}

bool IsPacketTypeSupported(PacketType type)
{
  switch (type)
  {
    case PacketType::Handshake:
    case PacketType::PeerDiscovery:
    case PacketType::ChunkDelta:
    case PacketType::WorldEvent: return true;
    default: return false;
  }
}

Packet MakeHandshakePacket(const HandshakePacketData& handshake)
{
  Packet packet = {};
  packet.type = PacketType::Handshake;
  AppendU32(&packet.payload, handshake.protocolVersion);
  AppendU32(&packet.payload, handshake.nodeId);
  AppendU64(&packet.payload, handshake.realmHash);
  return packet;
}

bool TryDecodeHandshakePacket(const Packet& packet, HandshakePacketData* handshake)
{
  if (handshake == nullptr) return false;
  if (packet.type != PacketType::Handshake) return false;
  if (packet.payload.size() != HANDSHAKE_PAYLOAD_SIZE) return false;

  handshake->protocolVersion = ReadU32(packet.payload, 0);
  handshake->nodeId = ReadU32(packet.payload, 4);
  handshake->realmHash = ReadU64(packet.payload, 8);
  return true;
}

uint32_t ComputePacketChecksum(const PacketHeader& header, const std::vector<uint8_t>& payload)
{
  std::vector<uint8_t> checksumHeader = {};
  checksumHeader.reserve(PACKET_HEADER_SIZE);
  AppendU32(&checksumHeader, header.magic);
  checksumHeader.push_back(header.version);
  checksumHeader.push_back(header.type);
  checksumHeader.push_back((uint8_t)(header.reserved & 0xff));
  checksumHeader.push_back((uint8_t)((header.reserved >> 8) & 0xff));
  AppendU32(&checksumHeader, header.payloadSize);
  AppendU32(&checksumHeader, 0);

  ENetBuffer buffers[2] = {};
  buffers[0].data = checksumHeader.data();
  buffers[0].dataLength = checksumHeader.size();
  buffers[1].data = payload.empty() ? nullptr : (void*)payload.data();
  buffers[1].dataLength = payload.size();
  return enet_crc32(buffers, payload.empty() ? 1 : 2);
}

std::vector<uint8_t> SerializePacket(const Packet& packet)
{
  PacketHeader header = {};
  header.magic = RUNTIME_PACKET_MAGIC;
  header.version = RUNTIME_PACKET_VERSION;
  header.type = (uint8_t)packet.type;
  header.payloadSize = (uint32_t)packet.payload.size();
  header.checksum = ComputePacketChecksum(header, packet.payload);

  std::vector<uint8_t> bytes = {};
  bytes.reserve(PACKET_HEADER_SIZE + packet.payload.size());
  AppendU32(&bytes, header.magic);
  bytes.push_back(header.version);
  bytes.push_back(header.type);
  bytes.push_back((uint8_t)(header.reserved & 0xff));
  bytes.push_back((uint8_t)((header.reserved >> 8) & 0xff));
  AppendU32(&bytes, header.payloadSize);
  AppendU32(&bytes, header.checksum);
  bytes.insert(bytes.end(), packet.payload.begin(), packet.payload.end());
  return bytes;
}

bool TryParsePacket(const std::vector<uint8_t>& bytes, Packet* packet)
{
  if (packet == nullptr) return false;
  if (bytes.size() < PACKET_HEADER_SIZE) return false;

  PacketHeader header = {};
  header.magic = ReadU32(bytes, 0);
  header.version = bytes[4];
  header.type = bytes[5];
  header.reserved = ReadU16(bytes, 6);
  header.payloadSize = ReadU32(bytes, 8);
  header.checksum = ReadU32(bytes, 12);

  if (header.magic != RUNTIME_PACKET_MAGIC) return false;
  if (header.version != RUNTIME_PACKET_VERSION) return false;
  if (bytes.size() != (size_t)(PACKET_HEADER_SIZE + header.payloadSize)) return false;

  PacketType packetType = (PacketType)header.type;
  if (!IsPacketTypeSupported(packetType)) return false;

  std::vector<uint8_t> payload = {};
  payload.assign(bytes.begin() + PACKET_HEADER_SIZE, bytes.end());
  if (ComputePacketChecksum(header, payload) != header.checksum) return false;

  packet->type = packetType;
  packet->checksum = header.checksum;
  packet->payload = std::move(payload);
  return true;
}

std::string DescribePacketType(PacketType type)
{
  switch (type)
  {
    case PacketType::Handshake: return "handshake";
    case PacketType::PeerDiscovery: return "peer_discovery";
    case PacketType::ChunkDelta: return "chunk_delta";
    case PacketType::WorldEvent: return "world_event";
    default: return "invalid";
  }
}