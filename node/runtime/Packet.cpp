#include "Packet.h"
#include "ProtocolVersion.h"

#include <enet/enet.h>

#include <cstring>

static constexpr uint32_t RUNTIME_PACKET_MAGIC = 0x4f524c4d;
static constexpr size_t PACKET_HEADER_SIZE = 16;
static constexpr size_t MAX_HOST_BYTES = 255;
static constexpr size_t MAX_SNAPSHOT_NODES = 512;

static void AppendU8(std::vector<uint8_t>* bytes, uint8_t value)
{
  if (bytes == nullptr) return;
  bytes->push_back(value);
}

static void AppendU16(std::vector<uint8_t>* bytes, uint16_t value)
{
  if (bytes == nullptr) return;
  bytes->push_back((uint8_t)(value & 0xff));
  bytes->push_back((uint8_t)((value >> 8) & 0xff));
}

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

static void AppendF32(std::vector<uint8_t>* bytes, float value)
{
  uint32_t rawValue = 0;
  memcpy(&rawValue, &value, sizeof(rawValue));
  AppendU32(bytes, rawValue);
}

static void AppendString(std::vector<uint8_t>* bytes, const std::string& value)
{
  if (bytes == nullptr) return;
  const size_t size = value.size() > MAX_HOST_BYTES ? MAX_HOST_BYTES : value.size();
  AppendU8(bytes, (uint8_t)size);
  bytes->insert(bytes->end(), value.begin(), value.begin() + size);
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

static float ReadF32(const std::vector<uint8_t>& bytes, size_t offset)
{
  const uint32_t rawValue = ReadU32(bytes, offset);
  float value = 0.0f;
  memcpy(&value, &rawValue, sizeof(value));
  return value;
}

static bool TryReadString(const std::vector<uint8_t>& bytes, size_t* offset, std::string* value)
{
  if (offset == nullptr || value == nullptr) return false;
  if (*offset >= bytes.size()) return false;
  const size_t size = bytes[*offset];
  *offset += 1;
  if (*offset + size > bytes.size()) return false;
  value->assign((const char*)&bytes[*offset], size);
  *offset += size;
  return true;
}

static void AppendWorldPosition(std::vector<uint8_t>* bytes, const RuntimeWorldPosition& position)
{
  AppendF32(bytes, position.x);
  AppendF32(bytes, position.y);
  AppendF32(bytes, position.z);
}

static RuntimeWorldPosition ReadWorldPosition(const std::vector<uint8_t>& bytes, size_t* offset)
{
  RuntimeWorldPosition position = {};
  position.x = ReadF32(bytes, *offset);
  *offset += 4;
  position.y = ReadF32(bytes, *offset);
  *offset += 4;
  position.z = ReadF32(bytes, *offset);
  *offset += 4;
  return position;
}

static void AppendInterestArea(std::vector<uint8_t>* bytes, const RuntimeInterestArea& area)
{
  AppendF32(bytes, area.radiusX);
  AppendF32(bytes, area.radiusY);
  AppendF32(bytes, area.radiusZ);
}

static RuntimeInterestArea ReadInterestArea(const std::vector<uint8_t>& bytes, size_t* offset)
{
  RuntimeInterestArea area = {};
  area.radiusX = ReadF32(bytes, *offset);
  *offset += 4;
  area.radiusY = ReadF32(bytes, *offset);
  *offset += 4;
  area.radiusZ = ReadF32(bytes, *offset);
  *offset += 4;
  return area;
}

static void AppendTopologyNode(std::vector<uint8_t>* bytes, const TopologyNodeState& node)
{
  AppendU32(bytes, node.nodeId);
  AppendU32(bytes, node.protocolVersion);
  AppendU64(bytes, node.realmHash);
  AppendU32(bytes, (uint32_t)node.peerAddress.port);
  AppendString(bytes, node.peerAddress.host);
  AppendWorldPosition(bytes, node.position);
  AppendInterestArea(bytes, node.interestArea);
  AppendU8(bytes, node.nodeRole);
  AppendU8(bytes, node.acceptsJoins);
}

static bool TryReadTopologyNode(const std::vector<uint8_t>& bytes, size_t* offset, TopologyNodeState* node)
{
  if (offset == nullptr || node == nullptr) return false;
  if (*offset + 20 > bytes.size()) return false;

  node->nodeId = ReadU32(bytes, *offset);
  *offset += 4;
  node->protocolVersion = ReadU32(bytes, *offset);
  *offset += 4;
  node->realmHash = ReadU64(bytes, *offset);
  *offset += 8;
  node->peerAddress.port = (int)ReadU32(bytes, *offset);
  *offset += 4;
  if (!TryReadString(bytes, offset, &node->peerAddress.host)) return false;
  if (*offset + 26 > bytes.size()) return false;
  node->position = ReadWorldPosition(bytes, offset);
  node->interestArea = ReadInterestArea(bytes, offset);
  node->nodeRole = bytes[*offset];
  *offset += 1;
  node->acceptsJoins = bytes[*offset];
  *offset += 1;
  return true;
}

bool IsPacketTypeSupported(PacketType type)
{
  switch (type)
  {
    case PacketType::Handshake:
    case PacketType::JoinRequest:
    case PacketType::JoinResponse:
    case PacketType::TopologySnapshot:
    case PacketType::PlayerSnapshot: return true;
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
  AppendWorldPosition(&packet.payload, handshake.position);
  AppendInterestArea(&packet.payload, handshake.interestArea);
  AppendU8(&packet.payload, handshake.nodeRole);
  AppendU8(&packet.payload, handshake.acceptsJoins);
  AppendU16(&packet.payload, handshake.reserved);
  return packet;
}

bool TryDecodeHandshakePacket(const Packet& packet, HandshakePacketData* handshake)
{
  if (handshake == nullptr) return false;
  if (packet.type != PacketType::Handshake) return false;
  if (packet.payload.size() != 38) return false;

  size_t offset = 0;
  handshake->protocolVersion = ReadU32(packet.payload, offset);
  offset += 4;
  handshake->nodeId = ReadU32(packet.payload, offset);
  offset += 4;
  handshake->realmHash = ReadU64(packet.payload, offset);
  offset += 8;
  handshake->position = ReadWorldPosition(packet.payload, &offset);
  handshake->interestArea = ReadInterestArea(packet.payload, &offset);
  handshake->nodeRole = packet.payload[offset++];
  handshake->acceptsJoins = packet.payload[offset++];
  handshake->reserved = ReadU16(packet.payload, offset);
  offset += 2;
  return offset == packet.payload.size();
}

Packet MakeJoinRequestPacket(const JoinRequestPacketData& joinRequest)
{
  Packet packet = {};
  packet.type = PacketType::JoinRequest;
  AppendU32(&packet.payload, joinRequest.requestingNodeId);
  AppendWorldPosition(&packet.payload, joinRequest.targetPosition);
  AppendU32(&packet.payload, joinRequest.maxCandidates);
  AppendU32(&packet.payload, joinRequest.maxHops);
  AppendU32(&packet.payload, joinRequest.requestToken);
  return packet;
}

bool TryDecodeJoinRequestPacket(const Packet& packet, JoinRequestPacketData* joinRequest)
{
  if (joinRequest == nullptr) return false;
  if (packet.type != PacketType::JoinRequest) return false;
  if (packet.payload.size() != 28) return false;

  size_t offset = 0;
  joinRequest->requestingNodeId = ReadU32(packet.payload, offset);
  offset += 4;
  joinRequest->targetPosition = ReadWorldPosition(packet.payload, &offset);
  joinRequest->maxCandidates = ReadU32(packet.payload, offset);
  offset += 4;
  joinRequest->maxHops = ReadU32(packet.payload, offset);
  offset += 4;
  joinRequest->requestToken = ReadU32(packet.payload, offset);
  offset += 4;
  return offset == packet.payload.size();
}

Packet MakeJoinResponsePacket(const JoinResponsePacketData& joinResponse)
{
  Packet packet = {};
  packet.type = PacketType::JoinResponse;
  AppendU32(&packet.payload, joinResponse.respondingNodeId);
  AppendU32(&packet.payload, joinResponse.requestToken);
  AppendWorldPosition(&packet.payload, joinResponse.resolvedPosition);
  AppendU32(&packet.payload, (uint32_t)joinResponse.candidates.size());
  for (const TopologyNodeState& candidate : joinResponse.candidates)
  {
    AppendTopologyNode(&packet.payload, candidate);
  }
  return packet;
}

bool TryDecodeJoinResponsePacket(const Packet& packet, JoinResponsePacketData* joinResponse, size_t maxCandidates)
{
  if (joinResponse == nullptr) return false;
  if (packet.type != PacketType::JoinResponse) return false;
  if (packet.payload.size() < 24) return false;

  size_t offset = 0;
  joinResponse->respondingNodeId = ReadU32(packet.payload, offset);
  offset += 4;
  joinResponse->requestToken = ReadU32(packet.payload, offset);
  offset += 4;
  joinResponse->resolvedPosition = ReadWorldPosition(packet.payload, &offset);
  const uint32_t count = ReadU32(packet.payload, offset);
  offset += 4;
  if (count > MAX_SNAPSHOT_NODES || count > maxCandidates) return false;

  joinResponse->candidates.clear();
  joinResponse->candidates.reserve(count);
  for (uint32_t i = 0; i < count; ++i)
  {
    TopologyNodeState node = {};
    if (!TryReadTopologyNode(packet.payload, &offset, &node)) return false;
    joinResponse->candidates.push_back(node);
  }

  return offset == packet.payload.size();
}

Packet MakeTopologySnapshotPacket(const TopologySnapshotPacketData& topologySnapshot)
{
  Packet packet = {};
  packet.type = PacketType::TopologySnapshot;
  AppendU32(&packet.payload, topologySnapshot.senderNodeId);
  AppendU32(&packet.payload, (uint32_t)topologySnapshot.nodes.size());
  for (const TopologyNodeState& node : topologySnapshot.nodes)
  {
    AppendTopologyNode(&packet.payload, node);
  }
  return packet;
}

bool TryDecodeTopologySnapshotPacket(const Packet& packet, TopologySnapshotPacketData* topologySnapshot, size_t maxNodes)
{
  if (topologySnapshot == nullptr) return false;
  if (packet.type != PacketType::TopologySnapshot) return false;
  if (packet.payload.size() < 8) return false;

  size_t offset = 0;
  topologySnapshot->senderNodeId = ReadU32(packet.payload, offset);
  offset += 4;
  const uint32_t count = ReadU32(packet.payload, offset);
  offset += 4;
  if (count > MAX_SNAPSHOT_NODES || count > maxNodes) return false;

  topologySnapshot->nodes.clear();
  topologySnapshot->nodes.reserve(count);
  for (uint32_t i = 0; i < count; ++i)
  {
    TopologyNodeState node = {};
    if (!TryReadTopologyNode(packet.payload, &offset, &node)) return false;
    topologySnapshot->nodes.push_back(node);
  }

  return offset == packet.payload.size();
}

Packet MakePlayerSnapshotPacket(const PlayerSnapshotPacketData& playerSnapshot)
{
  Packet packet = {};
  packet.type = PacketType::PlayerSnapshot;
  AppendU32(&packet.payload, playerSnapshot.nodeId);
  AppendWorldPosition(&packet.payload, playerSnapshot.nodePosition);
  AppendWorldPosition(&packet.payload, playerSnapshot.playerPosition);
  AppendF32(&packet.payload, playerSnapshot.yaw);
  AppendF32(&packet.payload, playerSnapshot.pitch);
  AppendU8(&packet.payload, playerSnapshot.active);
  return packet;
}

bool TryDecodePlayerSnapshotPacket(const Packet& packet, PlayerSnapshotPacketData* playerSnapshot)
{
  if (playerSnapshot == nullptr) return false;
  if (packet.type != PacketType::PlayerSnapshot) return false;
  if (packet.payload.size() != 37) return false;

  size_t offset = 0;
  playerSnapshot->nodeId = ReadU32(packet.payload, offset);
  offset += 4;
  playerSnapshot->nodePosition = ReadWorldPosition(packet.payload, &offset);
  playerSnapshot->playerPosition = ReadWorldPosition(packet.payload, &offset);
  playerSnapshot->yaw = ReadF32(packet.payload, offset);
  offset += 4;
  playerSnapshot->pitch = ReadF32(packet.payload, offset);
  offset += 4;
  playerSnapshot->active = packet.payload[offset++];
  return offset == packet.payload.size();
}

uint32_t ComputePacketChecksum(const PacketHeader& header, const std::vector<uint8_t>& payload)
{
  std::vector<uint8_t> checksumHeader = {};
  checksumHeader.reserve(PACKET_HEADER_SIZE);
  AppendU32(&checksumHeader, header.magic);
  AppendU8(&checksumHeader, header.version);
  AppendU8(&checksumHeader, header.type);
  AppendU16(&checksumHeader, header.reserved);
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
  AppendU8(&bytes, header.version);
  AppendU8(&bytes, header.type);
  AppendU16(&bytes, header.reserved);
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

  const PacketType type = (PacketType)header.type;
  if (!IsPacketTypeSupported(type)) return false;

  std::vector<uint8_t> payload = {};
  payload.assign(bytes.begin() + PACKET_HEADER_SIZE, bytes.end());
  if (ComputePacketChecksum(header, payload) != header.checksum) return false;

  packet->type = type;
  packet->checksum = header.checksum;
  packet->payload = std::move(payload);
  return true;
}

std::string DescribePacketType(PacketType type)
{
  switch (type)
  {
    case PacketType::Handshake: return "handshake";
    case PacketType::JoinRequest: return "join_request";
    case PacketType::JoinResponse: return "join_response";
    case PacketType::TopologySnapshot: return "topology_snapshot";
    case PacketType::PlayerSnapshot: return "player_snapshot";
    default: return "invalid";
  }
}
