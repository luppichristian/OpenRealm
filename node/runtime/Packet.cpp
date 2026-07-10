#include "Packet.h"

#include "ProtocolVersion.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
static constexpr uint32_t kPacketMagic = 0x4f52544d; // ORTM
static constexpr uint8_t kMaxHostBytes = 255;
static constexpr uint8_t kMaxAddressBytes = 127;
static constexpr uint16_t kMaxSignatureBytes = 255;
static constexpr uint32_t kRuntimeAuthDomainVersion = 1;

void AppendBytes(std::vector<uint8_t>* bytes, const void* data, size_t size)
{
  if (bytes == nullptr || data == nullptr || size == 0) return;
  const uint8_t* begin = static_cast<const uint8_t*>(data);
  bytes->insert(bytes->end(), begin, begin + size);
}

template<typename T>
void AppendValue(std::vector<uint8_t>* bytes, const T& value)
{
  AppendBytes(bytes, &value, sizeof(T));
}

void AppendString(std::vector<uint8_t>* bytes, const std::string& value, uint16_t maxBytes)
{
  const size_t cappedSize = std::min(value.size(), (size_t)maxBytes);
  const uint16_t encodedSize = (uint16_t)cappedSize;
  AppendValue(bytes, encodedSize);
  if (encodedSize > 0)
  {
    AppendBytes(bytes, value.data(), encodedSize);
  }
}

bool TryReadBytes(const std::vector<uint8_t>& bytes, size_t* offset, void* data, size_t size)
{
  if (offset == nullptr || data == nullptr || *offset + size > bytes.size()) return false;
  std::memcpy(data, bytes.data() + *offset, size);
  *offset += size;
  return true;
}

template<typename T>
bool TryReadValue(const std::vector<uint8_t>& bytes, size_t* offset, T* value)
{
  return TryReadBytes(bytes, offset, value, sizeof(T));
}

bool TryReadString(const std::vector<uint8_t>& bytes, size_t* offset, std::string* value, uint16_t maxBytes)
{
  if (value == nullptr) return false;
  uint16_t encodedSize = 0;
  if (!TryReadValue(bytes, offset, &encodedSize)) return false;
  if (encodedSize > maxBytes || *offset + encodedSize > bytes.size()) return false;
  value->assign(reinterpret_cast<const char*>(bytes.data() + *offset), encodedSize);
  *offset += encodedSize;
  return true;
}

uint32_t ComputeChecksum(PacketType type, uint8_t version, const std::vector<uint8_t>& payload)
{
  uint32_t checksum = 2166136261u;
  const uint8_t typeByte = (uint8_t)type;
  checksum = (checksum ^ typeByte) * 16777619u;
  checksum = (checksum ^ version) * 16777619u;
  for (uint8_t byte : payload)
  {
    checksum = (checksum ^ byte) * 16777619u;
  }
  return checksum;
}

void AppendPeerAddress(std::vector<uint8_t>* bytes, const RuntimePeerAddress& peerAddress)
{
  AppendString(bytes, peerAddress.host, kMaxHostBytes);
  uint16_t port = 0;
  if (peerAddress.port > 0)
  {
    const int boundedPort = peerAddress.port > 65535 ? 65535 : peerAddress.port;
    port = (uint16_t)boundedPort;
  }
  AppendValue(bytes, port);
}

bool TryReadPeerAddress(const std::vector<uint8_t>& bytes, size_t* offset, RuntimePeerAddress* peerAddress)
{
  if (peerAddress == nullptr) return false;
  uint16_t port = 0;
  if (!TryReadString(bytes, offset, &peerAddress->host, kMaxHostBytes)) return false;
  if (!TryReadValue(bytes, offset, &port)) return false;
  peerAddress->port = (int)port;
  return true;
}

void AppendWorldPosition(std::vector<uint8_t>* bytes, const RuntimeWorldPosition& position)
{
  AppendValue(bytes, position.x);
  AppendValue(bytes, position.y);
  AppendValue(bytes, position.z);
}

bool TryReadWorldPosition(const std::vector<uint8_t>& bytes, size_t* offset, RuntimeWorldPosition* position)
{
  return position != nullptr
      && TryReadValue(bytes, offset, &position->x)
      && TryReadValue(bytes, offset, &position->y)
      && TryReadValue(bytes, offset, &position->z);
}

void AppendInterestArea(std::vector<uint8_t>* bytes, const RuntimeInterestArea& area)
{
  AppendValue(bytes, area.radiusX);
  AppendValue(bytes, area.radiusY);
  AppendValue(bytes, area.radiusZ);
}

bool TryReadInterestArea(const std::vector<uint8_t>& bytes, size_t* offset, RuntimeInterestArea* area)
{
  return area != nullptr
      && TryReadValue(bytes, offset, &area->radiusX)
      && TryReadValue(bytes, offset, &area->radiusY)
      && TryReadValue(bytes, offset, &area->radiusZ);
}

void AppendPeerProof(std::vector<uint8_t>* bytes, const PacketPeerProof& proof)
{
  AppendValue(bytes, proof.sessionId);
  AppendValue(bytes, proof.sequence);
  AppendString(bytes, proof.signatureHex, kMaxSignatureBytes);
}

bool TryReadPeerProof(const std::vector<uint8_t>& bytes, size_t* offset, PacketPeerProof* proof)
{
  return proof != nullptr
      && TryReadValue(bytes, offset, &proof->sessionId)
      && TryReadValue(bytes, offset, &proof->sequence)
      && TryReadString(bytes, offset, &proof->signatureHex, kMaxSignatureBytes);
}

void AppendUnsignedPeerProof(std::vector<uint8_t>* bytes, const PacketPeerProof& proof)
{
  AppendValue(bytes, proof.sessionId);
  AppendValue(bytes, proof.sequence);
}

void AppendTopologyNode(std::vector<uint8_t>* bytes, const TopologyNodeState& node)
{
  AppendValue(bytes, node.sessionId);
  AppendValue(bytes, node.protocolVersion);
  AppendValue(bytes, node.realmHash);
  AppendPeerAddress(bytes, node.peerAddress);
  AppendWorldPosition(bytes, node.position);
  AppendInterestArea(bytes, node.interestArea);
  AppendValue(bytes, node.nodeRole);
  AppendValue(bytes, node.acceptsJoins);
}

bool TryReadTopologyNode(const std::vector<uint8_t>& bytes, size_t* offset, TopologyNodeState* node)
{
  return node != nullptr
      && TryReadValue(bytes, offset, &node->sessionId)
      && TryReadValue(bytes, offset, &node->protocolVersion)
      && TryReadValue(bytes, offset, &node->realmHash)
      && TryReadPeerAddress(bytes, offset, &node->peerAddress)
      && TryReadWorldPosition(bytes, offset, &node->position)
      && TryReadInterestArea(bytes, offset, &node->interestArea)
      && TryReadValue(bytes, offset, &node->nodeRole)
      && TryReadValue(bytes, offset, &node->acceptsJoins);
}

bool BuildSigningMessage(PacketType type, const std::vector<uint8_t>& payload, std::vector<uint8_t>* messageBytes)
{
  if (messageBytes == nullptr) return false;
  messageBytes->clear();
  static constexpr char kDomain[] = "openrealm-runtime-auth";
  AppendBytes(messageBytes, kDomain, sizeof(kDomain) - 1);
  AppendValue(messageBytes, kRuntimeAuthDomainVersion);
  const uint8_t typeByte = (uint8_t)type;
  AppendValue(messageBytes, typeByte);
  if (!payload.empty())
  {
    AppendBytes(messageBytes, payload.data(), payload.size());
  }
  return true;
}

bool BuildHandshakeUnsignedPayload(const HandshakePacketData& handshake, std::vector<uint8_t>* payload)
{
  if (payload == nullptr) return false;
  payload->clear();
  AppendUnsignedPeerProof(payload, handshake.proof);
  AppendValue(payload, handshake.protocolVersion);
  AppendValue(payload, handshake.realmHash);
  AppendWorldPosition(payload, handshake.position);
  AppendInterestArea(payload, handshake.interestArea);
  AppendValue(payload, handshake.nodeRole);
  AppendValue(payload, handshake.acceptsJoins);
  AppendValue(payload, handshake.reserved);
  AppendValue(payload, handshake.challengeNonce);
  AppendString(payload, handshake.signerAddress, kMaxAddressBytes);
  return true;
}

bool BuildJoinRequestUnsignedPayload(const JoinRequestPacketData& joinRequest, std::vector<uint8_t>* payload)
{
  if (payload == nullptr) return false;
  payload->clear();
  AppendUnsignedPeerProof(payload, joinRequest.proof);
  AppendWorldPosition(payload, joinRequest.targetPosition);
  AppendValue(payload, joinRequest.maxCandidates);
  AppendValue(payload, joinRequest.maxHops);
  AppendValue(payload, joinRequest.requestToken);
  return true;
}

bool BuildJoinResponseUnsignedPayload(const JoinResponsePacketData& joinResponse, std::vector<uint8_t>* payload)
{
  if (payload == nullptr) return false;
  payload->clear();
  AppendUnsignedPeerProof(payload, joinResponse.proof);
  AppendValue(payload, joinResponse.requestToken);
  AppendWorldPosition(payload, joinResponse.resolvedPosition);
  const uint16_t count = (uint16_t)std::min(joinResponse.candidates.size(), (size_t)std::numeric_limits<uint16_t>::max());
  AppendValue(payload, count);
  for (uint16_t i = 0; i < count; ++i)
  {
    AppendTopologyNode(payload, joinResponse.candidates[i]);
  }
  return true;
}

bool BuildTopologySnapshotUnsignedPayload(const TopologySnapshotPacketData& topologySnapshot, std::vector<uint8_t>* payload)
{
  if (payload == nullptr) return false;
  payload->clear();
  AppendUnsignedPeerProof(payload, topologySnapshot.proof);
  const uint16_t count = (uint16_t)std::min(topologySnapshot.nodes.size(), (size_t)std::numeric_limits<uint16_t>::max());
  AppendValue(payload, count);
  for (uint16_t i = 0; i < count; ++i)
  {
    AppendTopologyNode(payload, topologySnapshot.nodes[i]);
  }
  return true;
}

bool BuildPlayerSnapshotUnsignedPayload(const PlayerSnapshotPacketData& playerSnapshot, std::vector<uint8_t>* payload)
{
  if (payload == nullptr) return false;
  payload->clear();
  AppendUnsignedPeerProof(payload, playerSnapshot.proof);
  AppendValue(payload, playerSnapshot.tickMs);
  AppendWorldPosition(payload, playerSnapshot.nodePosition);
  AppendWorldPosition(payload, playerSnapshot.playerPosition);
  AppendValue(payload, playerSnapshot.yaw);
  AppendValue(payload, playerSnapshot.pitch);
  AppendValue(payload, playerSnapshot.active);
  return true;
}

bool BuildChallengeRequestUnsignedPayload(const ChallengeRequestPacketData& challengeRequest, std::vector<uint8_t>* payload)
{
  if (payload == nullptr) return false;
  payload->clear();
  AppendUnsignedPeerProof(payload, challengeRequest.proof);
  AppendValue(payload, challengeRequest.challengeNonce);
  return true;
}

bool BuildChallengeResponseUnsignedPayload(const ChallengeResponsePacketData& challengeResponse, std::vector<uint8_t>* payload)
{
  if (payload == nullptr) return false;
  payload->clear();
  AppendUnsignedPeerProof(payload, challengeResponse.proof);
  AppendValue(payload, challengeResponse.challengeNonce);
  return true;
}

Packet BuildPacket(PacketType type, const std::vector<uint8_t>& payload)
{
  Packet packet = {};
  packet.type = type;
  packet.version = kRuntimePacketVersion;
  packet.payload = payload;
  packet.checksum = ComputeChecksum(packet.type, packet.version, packet.payload);
  return packet;
}
} // namespace

std::vector<uint8_t> SerializePacket(const Packet& packet)
{
  std::vector<uint8_t> bytes = {};
  AppendValue(&bytes, kPacketMagic);
  AppendValue(&bytes, packet.version);
  const uint8_t typeByte = (uint8_t)packet.type;
  AppendValue(&bytes, typeByte);
  const uint32_t payloadSize = (uint32_t)packet.payload.size();
  AppendValue(&bytes, payloadSize);
  AppendValue(&bytes, packet.checksum);
  if (!packet.payload.empty())
  {
    AppendBytes(&bytes, packet.payload.data(), packet.payload.size());
  }
  return bytes;
}

bool TryParsePacket(const std::vector<uint8_t>& bytes, Packet* packet)
{
  if (packet == nullptr) return false;

  size_t offset = 0;
  uint32_t packetMagic = 0;
  uint8_t packetVersion = 0;
  uint8_t packetType = 0;
  uint32_t payloadSize = 0;
  uint32_t packetChecksum = 0;
  if (!TryReadValue(bytes, &offset, &packetMagic)
      || !TryReadValue(bytes, &offset, &packetVersion)
      || !TryReadValue(bytes, &offset, &packetType)
      || !TryReadValue(bytes, &offset, &payloadSize)
      || !TryReadValue(bytes, &offset, &packetChecksum))
  {
    return false;
  }
  if (packetMagic != kPacketMagic || packetVersion != kRuntimePacketVersion || offset + payloadSize != bytes.size()) return false;

  packet->type = (PacketType)packetType;
  packet->version = packetVersion;
  packet->checksum = packetChecksum;
  packet->payload.assign(bytes.begin() + (long long)offset, bytes.end());
  return ComputeChecksum(packet->type, packet->version, packet->payload) == packet->checksum;
}

Packet MakeHandshakePacket(const HandshakePacketData& handshake)
{
  std::vector<uint8_t> payload = {};
  AppendPeerProof(&payload, handshake.proof);
  AppendValue(&payload, handshake.protocolVersion);
  AppendValue(&payload, handshake.realmHash);
  AppendWorldPosition(&payload, handshake.position);
  AppendInterestArea(&payload, handshake.interestArea);
  AppendValue(&payload, handshake.nodeRole);
  AppendValue(&payload, handshake.acceptsJoins);
  AppendValue(&payload, handshake.reserved);
  AppendValue(&payload, handshake.challengeNonce);
  AppendString(&payload, handshake.signerAddress, kMaxAddressBytes);
  return BuildPacket(PacketType::Handshake, payload);
}

Packet MakeJoinRequestPacket(const JoinRequestPacketData& joinRequest)
{
  std::vector<uint8_t> payload = {};
  AppendPeerProof(&payload, joinRequest.proof);
  AppendWorldPosition(&payload, joinRequest.targetPosition);
  AppendValue(&payload, joinRequest.maxCandidates);
  AppendValue(&payload, joinRequest.maxHops);
  AppendValue(&payload, joinRequest.requestToken);
  return BuildPacket(PacketType::JoinRequest, payload);
}

Packet MakeJoinResponsePacket(const JoinResponsePacketData& joinResponse)
{
  std::vector<uint8_t> payload = {};
  AppendPeerProof(&payload, joinResponse.proof);
  AppendValue(&payload, joinResponse.requestToken);
  AppendWorldPosition(&payload, joinResponse.resolvedPosition);
  const uint16_t count = (uint16_t)std::min(joinResponse.candidates.size(), (size_t)std::numeric_limits<uint16_t>::max());
  AppendValue(&payload, count);
  for (uint16_t i = 0; i < count; ++i)
  {
    AppendTopologyNode(&payload, joinResponse.candidates[i]);
  }
  return BuildPacket(PacketType::JoinResponse, payload);
}

Packet MakeTopologySnapshotPacket(const TopologySnapshotPacketData& topologySnapshot)
{
  std::vector<uint8_t> payload = {};
  AppendPeerProof(&payload, topologySnapshot.proof);
  const uint16_t count = (uint16_t)std::min(topologySnapshot.nodes.size(), (size_t)std::numeric_limits<uint16_t>::max());
  AppendValue(&payload, count);
  for (uint16_t i = 0; i < count; ++i)
  {
    AppendTopologyNode(&payload, topologySnapshot.nodes[i]);
  }
  return BuildPacket(PacketType::TopologySnapshot, payload);
}

Packet MakePlayerSnapshotPacket(const PlayerSnapshotPacketData& playerSnapshot)
{
  std::vector<uint8_t> payload = {};
  AppendPeerProof(&payload, playerSnapshot.proof);
  AppendValue(&payload, playerSnapshot.tickMs);
  AppendWorldPosition(&payload, playerSnapshot.nodePosition);
  AppendWorldPosition(&payload, playerSnapshot.playerPosition);
  AppendValue(&payload, playerSnapshot.yaw);
  AppendValue(&payload, playerSnapshot.pitch);
  AppendValue(&payload, playerSnapshot.active);
  return BuildPacket(PacketType::PlayerSnapshot, payload);
}

Packet MakeChallengeRequestPacket(const ChallengeRequestPacketData& challengeRequest)
{
  std::vector<uint8_t> payload = {};
  AppendPeerProof(&payload, challengeRequest.proof);
  AppendValue(&payload, challengeRequest.challengeNonce);
  return BuildPacket(PacketType::ChallengeRequest, payload);
}

Packet MakeChallengeResponsePacket(const ChallengeResponsePacketData& challengeResponse)
{
  std::vector<uint8_t> payload = {};
  AppendPeerProof(&payload, challengeResponse.proof);
  AppendValue(&payload, challengeResponse.challengeNonce);
  return BuildPacket(PacketType::ChallengeResponse, payload);
}

bool TryDecodeHandshakePacket(const Packet& packet, HandshakePacketData* handshake)
{
  if (packet.type != PacketType::Handshake || handshake == nullptr) return false;
  size_t offset = 0;
  if (!TryReadPeerProof(packet.payload, &offset, &handshake->proof)
      || !TryReadValue(packet.payload, &offset, &handshake->protocolVersion)
      || !TryReadValue(packet.payload, &offset, &handshake->realmHash)
      || !TryReadWorldPosition(packet.payload, &offset, &handshake->position)
      || !TryReadInterestArea(packet.payload, &offset, &handshake->interestArea)
      || !TryReadValue(packet.payload, &offset, &handshake->nodeRole)
      || !TryReadValue(packet.payload, &offset, &handshake->acceptsJoins)
      || !TryReadValue(packet.payload, &offset, &handshake->reserved)
      || !TryReadValue(packet.payload, &offset, &handshake->challengeNonce)
      || !TryReadString(packet.payload, &offset, &handshake->signerAddress, kMaxAddressBytes))
  {
    return false;
  }
  return offset == packet.payload.size();
}

bool TryDecodeJoinRequestPacket(const Packet& packet, JoinRequestPacketData* joinRequest)
{
  if (packet.type != PacketType::JoinRequest || joinRequest == nullptr) return false;
  size_t offset = 0;
  if (!TryReadPeerProof(packet.payload, &offset, &joinRequest->proof)
      || !TryReadWorldPosition(packet.payload, &offset, &joinRequest->targetPosition)
      || !TryReadValue(packet.payload, &offset, &joinRequest->maxCandidates)
      || !TryReadValue(packet.payload, &offset, &joinRequest->maxHops)
      || !TryReadValue(packet.payload, &offset, &joinRequest->requestToken))
  {
    return false;
  }
  return offset == packet.payload.size();
}

bool TryDecodeJoinResponsePacket(const Packet& packet, JoinResponsePacketData* joinResponse, size_t maxCandidates)
{
  if (packet.type != PacketType::JoinResponse || joinResponse == nullptr) return false;
  size_t offset = 0;
  uint16_t count = 0;
  if (!TryReadPeerProof(packet.payload, &offset, &joinResponse->proof)
      || !TryReadValue(packet.payload, &offset, &joinResponse->requestToken)
      || !TryReadWorldPosition(packet.payload, &offset, &joinResponse->resolvedPosition)
      || !TryReadValue(packet.payload, &offset, &count)
      || count > maxCandidates)
  {
    return false;
  }
  joinResponse->candidates.clear();
  for (uint16_t i = 0; i < count; ++i)
  {
    TopologyNodeState node = {};
    if (!TryReadTopologyNode(packet.payload, &offset, &node)) return false;
    joinResponse->candidates.push_back(node);
  }
  return offset == packet.payload.size();
}

bool TryDecodeTopologySnapshotPacket(const Packet& packet, TopologySnapshotPacketData* topologySnapshot, size_t maxNodes)
{
  if (packet.type != PacketType::TopologySnapshot || topologySnapshot == nullptr) return false;
  size_t offset = 0;
  uint16_t count = 0;
  if (!TryReadPeerProof(packet.payload, &offset, &topologySnapshot->proof)
      || !TryReadValue(packet.payload, &offset, &count)
      || count > maxNodes)
  {
    return false;
  }
  topologySnapshot->nodes.clear();
  for (uint16_t i = 0; i < count; ++i)
  {
    TopologyNodeState node = {};
    if (!TryReadTopologyNode(packet.payload, &offset, &node)) return false;
    topologySnapshot->nodes.push_back(node);
  }
  return offset == packet.payload.size();
}

bool TryDecodePlayerSnapshotPacket(const Packet& packet, PlayerSnapshotPacketData* playerSnapshot)
{
  if (packet.type != PacketType::PlayerSnapshot || playerSnapshot == nullptr) return false;
  size_t offset = 0;
  if (!TryReadPeerProof(packet.payload, &offset, &playerSnapshot->proof)
      || !TryReadValue(packet.payload, &offset, &playerSnapshot->tickMs)
      || !TryReadWorldPosition(packet.payload, &offset, &playerSnapshot->nodePosition)
      || !TryReadWorldPosition(packet.payload, &offset, &playerSnapshot->playerPosition)
      || !TryReadValue(packet.payload, &offset, &playerSnapshot->yaw)
      || !TryReadValue(packet.payload, &offset, &playerSnapshot->pitch)
      || !TryReadValue(packet.payload, &offset, &playerSnapshot->active))
  {
    return false;
  }
  return offset == packet.payload.size();
}

bool TryDecodeChallengeRequestPacket(const Packet& packet, ChallengeRequestPacketData* challengeRequest)
{
  if (packet.type != PacketType::ChallengeRequest || challengeRequest == nullptr) return false;
  size_t offset = 0;
  if (!TryReadPeerProof(packet.payload, &offset, &challengeRequest->proof)
      || !TryReadValue(packet.payload, &offset, &challengeRequest->challengeNonce))
  {
    return false;
  }
  return offset == packet.payload.size();
}

bool TryDecodeChallengeResponsePacket(const Packet& packet, ChallengeResponsePacketData* challengeResponse)
{
  if (packet.type != PacketType::ChallengeResponse || challengeResponse == nullptr) return false;
  size_t offset = 0;
  if (!TryReadPeerProof(packet.payload, &offset, &challengeResponse->proof)
      || !TryReadValue(packet.payload, &offset, &challengeResponse->challengeNonce))
  {
    return false;
  }
  return offset == packet.payload.size();
}

bool BuildHandshakeSigningMessage(const HandshakePacketData& handshake, std::vector<uint8_t>* messageBytes)
{
  std::vector<uint8_t> payload = {};
  return BuildHandshakeUnsignedPayload(handshake, &payload) && BuildSigningMessage(PacketType::Handshake, payload, messageBytes);
}

bool BuildJoinRequestSigningMessage(const JoinRequestPacketData& joinRequest, std::vector<uint8_t>* messageBytes)
{
  std::vector<uint8_t> payload = {};
  return BuildJoinRequestUnsignedPayload(joinRequest, &payload) && BuildSigningMessage(PacketType::JoinRequest, payload, messageBytes);
}

bool BuildJoinResponseSigningMessage(const JoinResponsePacketData& joinResponse, std::vector<uint8_t>* messageBytes)
{
  std::vector<uint8_t> payload = {};
  return BuildJoinResponseUnsignedPayload(joinResponse, &payload) && BuildSigningMessage(PacketType::JoinResponse, payload, messageBytes);
}

bool BuildTopologySnapshotSigningMessage(const TopologySnapshotPacketData& topologySnapshot, std::vector<uint8_t>* messageBytes)
{
  std::vector<uint8_t> payload = {};
  return BuildTopologySnapshotUnsignedPayload(topologySnapshot, &payload) && BuildSigningMessage(PacketType::TopologySnapshot, payload, messageBytes);
}

bool BuildPlayerSnapshotSigningMessage(const PlayerSnapshotPacketData& playerSnapshot, std::vector<uint8_t>* messageBytes)
{
  std::vector<uint8_t> payload = {};
  return BuildPlayerSnapshotUnsignedPayload(playerSnapshot, &payload) && BuildSigningMessage(PacketType::PlayerSnapshot, payload, messageBytes);
}

bool BuildChallengeRequestSigningMessage(const ChallengeRequestPacketData& challengeRequest, std::vector<uint8_t>* messageBytes)
{
  std::vector<uint8_t> payload = {};
  return BuildChallengeRequestUnsignedPayload(challengeRequest, &payload) && BuildSigningMessage(PacketType::ChallengeRequest, payload, messageBytes);
}

bool BuildChallengeResponseSigningMessage(const ChallengeResponsePacketData& challengeResponse, std::vector<uint8_t>* messageBytes)
{
  std::vector<uint8_t> payload = {};
  return BuildChallengeResponseUnsignedPayload(challengeResponse, &payload) && BuildSigningMessage(PacketType::ChallengeResponse, payload, messageBytes);
}

std::string DescribePacketType(PacketType type)
{
  switch (type)
  {
    case PacketType::Handshake:         return "handshake";
    case PacketType::JoinRequest:       return "join_request";
    case PacketType::JoinResponse:      return "join_response";
    case PacketType::TopologySnapshot:  return "topology_snapshot";
    case PacketType::PlayerSnapshot:    return "player_snapshot";
    case PacketType::ChallengeRequest:  return "challenge_request";
    case PacketType::ChallengeResponse: return "challenge_response";
    default:                            return "unknown";
  }
}
