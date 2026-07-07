#include "Packet.h"
#include "ProtocolVersion.h"

#include <enet/enet.h>

#include <cstring>

static constexpr uint32_t RUNTIME_PACKET_MAGIC = 0x4f524c4d;
static constexpr size_t PACKET_HEADER_SIZE = 16;
static constexpr size_t HANDSHAKE_PAYLOAD_SIZE = 16;
static constexpr size_t CHUNK_INTEREST_PAYLOAD_SIZE = 16;
static constexpr size_t WORLD_EVENT_PAYLOAD_SIZE = 58;
static constexpr size_t MAX_PEER_DISCOVERY_NODES = 256;
static constexpr size_t MAX_PEER_DISCOVERY_HOST_BYTES = 255;

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

static void AppendI32(std::vector<uint8_t>* bytes, int32_t value)
{
  AppendU32(bytes, (uint32_t)value);
}

static void AppendF32(std::vector<uint8_t>* bytes, float value)
{
  uint32_t rawValue = 0;
  static_assert(sizeof(rawValue) == sizeof(value));
  memcpy(&rawValue, &value, sizeof(rawValue));
  AppendU32(bytes, rawValue);
}

static void AppendString(std::vector<uint8_t>* bytes, const std::string& value)
{
  if (bytes == nullptr) return;

  const size_t stringSize = value.size() > MAX_PEER_DISCOVERY_HOST_BYTES
      ? MAX_PEER_DISCOVERY_HOST_BYTES
      : value.size();
  bytes->push_back((uint8_t)stringSize);
  bytes->insert(bytes->end(), value.begin(), value.begin() + stringSize);
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

static int32_t ReadI32(const std::vector<uint8_t>& bytes, size_t offset)
{
  return (int32_t)ReadU32(bytes, offset);
}

static float ReadF32(const std::vector<uint8_t>& bytes, size_t offset)
{
  const uint32_t rawValue = ReadU32(bytes, offset);
  float value = 0.0f;
  static_assert(sizeof(rawValue) == sizeof(value));
  memcpy(&value, &rawValue, sizeof(value));
  return value;
}

static bool TryReadString(const std::vector<uint8_t>& bytes, size_t* offset, std::string* value)
{
  if (offset == nullptr || value == nullptr) return false;
  if (*offset >= bytes.size()) return false;

  const size_t stringSize = bytes[*offset];
  *offset += 1;
  if (*offset + stringSize > bytes.size()) return false;

  value->assign((const char*)&bytes[*offset], stringSize);
  *offset += stringSize;
  return true;
}

bool IsPacketTypeSupported(PacketType type)
{
  switch (type)
  {
    case PacketType::Handshake:
    case PacketType::PeerDiscovery:
    case PacketType::ChunkInterest:
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

Packet MakePeerDiscoveryPacket(const PeerDiscoveryPacketData& peerDiscovery)
{
  Packet packet = {};
  packet.type = PacketType::PeerDiscovery;
  AppendU32(&packet.payload, peerDiscovery.requestingNodeId);
  AppendU32(&packet.payload, (uint32_t)peerDiscovery.nodes.size());

  for (const PeerDiscoveryNodeState& node : peerDiscovery.nodes)
  {
    AppendU32(&packet.payload, node.nodeId);
    AppendU32(&packet.payload, node.protocolVersion);
    AppendU64(&packet.payload, node.realmHash);
    AppendU32(&packet.payload, (uint32_t)node.peerAddress.port);
    AppendString(&packet.payload, node.peerAddress.host);
  }

  return packet;
}

bool TryDecodePeerDiscoveryPacket(const Packet& packet, PeerDiscoveryPacketData* peerDiscovery, size_t maxNodes)
{
  if (peerDiscovery == nullptr) return false;
  if (packet.type != PacketType::PeerDiscovery) return false;
  if (packet.payload.size() < 8) return false;

  size_t offset = 0;
  peerDiscovery->requestingNodeId = ReadU32(packet.payload, offset);
  offset += 4;

  const uint32_t nodeCount = ReadU32(packet.payload, offset);
  offset += 4;
  if (nodeCount > MAX_PEER_DISCOVERY_NODES) return false;
  if ((size_t)nodeCount > maxNodes) return false;

  peerDiscovery->nodes.clear();
  peerDiscovery->nodes.reserve(nodeCount);

  for (uint32_t i = 0; i < nodeCount; ++i)
  {
    if (offset + 20 > packet.payload.size()) return false;

    PeerDiscoveryNodeState node = {};
    node.nodeId = ReadU32(packet.payload, offset);
    offset += 4;
    node.protocolVersion = ReadU32(packet.payload, offset);
    offset += 4;
    node.realmHash = ReadU64(packet.payload, offset);
    offset += 8;
    node.peerAddress.port = (int)ReadU32(packet.payload, offset);
    offset += 4;
    if (!TryReadString(packet.payload, &offset, &node.peerAddress.host)) return false;

    peerDiscovery->nodes.push_back(node);
  }

  return offset == packet.payload.size();
}

Packet MakeChunkInterestPacket(const ChunkInterestPacketData& chunkInterest)
{
  Packet packet = {};
  packet.type = PacketType::ChunkInterest;
  AppendU32(&packet.payload, chunkInterest.nodeId);
  AppendI32(&packet.payload, chunkInterest.chunkX);
  AppendI32(&packet.payload, chunkInterest.chunkY);
  AppendU32(&packet.payload, chunkInterest.radius);
  return packet;
}

bool TryDecodeChunkInterestPacket(const Packet& packet, ChunkInterestPacketData* chunkInterest)
{
  if (chunkInterest == nullptr) return false;
  if (packet.type != PacketType::ChunkInterest) return false;
  if (packet.payload.size() != CHUNK_INTEREST_PAYLOAD_SIZE) return false;

  chunkInterest->nodeId = ReadU32(packet.payload, 0);
  chunkInterest->chunkX = ReadI32(packet.payload, 4);
  chunkInterest->chunkY = ReadI32(packet.payload, 8);
  chunkInterest->radius = ReadU32(packet.payload, 12);
  return true;
}

Packet MakeWorldEventPacket(const WorldEventPacketData& worldEvent)
{
  Packet packet = {};
  packet.type = PacketType::WorldEvent;
  AppendU32(&packet.payload, worldEvent.nodeId);
  packet.payload.push_back(worldEvent.event.type);
  AppendU32(&packet.payload, (uint32_t)worldEvent.event.playerId);
  packet.payload.push_back(worldEvent.event.voxelValue);
  AppendI32(&packet.payload, worldEvent.event.voxelX);
  AppendI32(&packet.payload, worldEvent.event.voxelY);
  AppendI32(&packet.payload, worldEvent.event.voxelZ);
  AppendF32(&packet.payload, worldEvent.event.playerX);
  AppendF32(&packet.payload, worldEvent.event.playerY);
  AppendF32(&packet.payload, worldEvent.event.playerZ);
  AppendF32(&packet.payload, worldEvent.event.playerYaw);
  AppendF32(&packet.payload, worldEvent.event.playerPitch);
  AppendF32(&packet.payload, worldEvent.event.moveX);
  AppendF32(&packet.payload, worldEvent.event.moveY);
  AppendF32(&packet.payload, worldEvent.event.lookDeltaX);
  AppendF32(&packet.payload, worldEvent.event.lookDeltaY);
  return packet;
}

bool TryDecodeWorldEventPacket(const Packet& packet, WorldEventPacketData* worldEvent)
{
  if (worldEvent == nullptr) return false;
  if (packet.type != PacketType::WorldEvent) return false;
  if (packet.payload.size() != WORLD_EVENT_PAYLOAD_SIZE) return false;

  size_t offset = 0;
  worldEvent->nodeId = ReadU32(packet.payload, offset);
  offset += 4;
  worldEvent->event.type = packet.payload[offset];
  offset += 1;
  worldEvent->event.playerId = (int)ReadU32(packet.payload, offset);
  offset += 4;
  worldEvent->event.voxelValue = packet.payload[offset];
  offset += 1;
  worldEvent->event.voxelX = ReadI32(packet.payload, offset);
  offset += 4;
  worldEvent->event.voxelY = ReadI32(packet.payload, offset);
  offset += 4;
  worldEvent->event.voxelZ = ReadI32(packet.payload, offset);
  offset += 4;
  worldEvent->event.playerX = ReadF32(packet.payload, offset);
  offset += 4;
  worldEvent->event.playerY = ReadF32(packet.payload, offset);
  offset += 4;
  worldEvent->event.playerZ = ReadF32(packet.payload, offset);
  offset += 4;
  worldEvent->event.playerYaw = ReadF32(packet.payload, offset);
  offset += 4;
  worldEvent->event.playerPitch = ReadF32(packet.payload, offset);
  offset += 4;
  worldEvent->event.moveX = ReadF32(packet.payload, offset);
  offset += 4;
  worldEvent->event.moveY = ReadF32(packet.payload, offset);
  offset += 4;
  worldEvent->event.lookDeltaX = ReadF32(packet.payload, offset);
  offset += 4;
  worldEvent->event.lookDeltaY = ReadF32(packet.payload, offset);
  offset += 4;
  return offset == packet.payload.size();
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
  header.version = kRuntimePacketVersion;
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
  if (header.version != kRuntimePacketVersion) return false;
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
    case PacketType::ChunkInterest: return "chunk_interest";
    case PacketType::ChunkDelta: return "chunk_delta";
    case PacketType::WorldEvent: return "world_event";
    default: return "invalid";
  }
}
