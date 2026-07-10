#pragma once

#include "RuntimeClient.h"
#include "RuntimeWorldPosition.h"

#include <cstdint>
#include <string>
#include <vector>

enum class PacketType : uint8_t
{
  Handshake = 1,
  JoinRequest = 2,
  JoinResponse = 3,
  TopologySnapshot = 4,
  PlayerSnapshot = 5,
  ChallengeRequest = 6,
  ChallengeResponse = 7,
};

struct PacketPeerProof
{
  uint64_t sessionId = 0;
  uint32_t sequence = 0;
  std::string signatureHex = {};
};

struct HandshakePacketData
{
  PacketPeerProof proof = {};
  uint32_t protocolVersion = 0;
  uint64_t realmHash = 0;
  RuntimeWorldPosition position = {};
  RuntimeInterestArea interestArea = {};
  uint8_t nodeRole = 0;
  uint8_t acceptsJoins = 0;
  uint16_t reserved = 0;
  uint64_t challengeNonce = 0;
  std::string signerAddress = {};
};

struct JoinRequestPacketData
{
  PacketPeerProof proof = {};
  RuntimeWorldPosition targetPosition = {};
  uint32_t maxCandidates = 0;
  uint32_t maxHops = 0;
  uint64_t requestToken = 0;
};

struct ChallengeRequestPacketData
{
  PacketPeerProof proof = {};
  uint64_t challengeNonce = 0;
};

struct ChallengeResponsePacketData
{
  PacketPeerProof proof = {};
  uint64_t challengeNonce = 0;
};

struct TopologyNodeState
{
  uint64_t sessionId = 0;
  uint32_t protocolVersion = 0;
  uint64_t realmHash = 0;
  RuntimePeerAddress peerAddress = {};
  RuntimeWorldPosition position = {};
  RuntimeInterestArea interestArea = {};
  uint8_t nodeRole = 0;
  uint8_t acceptsJoins = 0;
};

struct JoinResponsePacketData
{
  PacketPeerProof proof = {};
  uint64_t requestToken = 0;
  RuntimeWorldPosition resolvedPosition = {};
  std::vector<TopologyNodeState> candidates = {};
};

struct TopologySnapshotPacketData
{
  PacketPeerProof proof = {};
  std::vector<TopologyNodeState> nodes = {};
};

struct PlayerSnapshotPacketData
{
  PacketPeerProof proof = {};
  uint32_t tickMs = 0;
  RuntimeWorldPosition nodePosition = {};
  RuntimeWorldPosition playerPosition = {};
  float yaw = 0.0f;
  float pitch = 0.0f;
  uint8_t active = 0;
};

struct Packet
{
  PacketType type = PacketType::Handshake;
  uint8_t version = 0;
  uint32_t checksum = 0;
  std::vector<uint8_t> payload = {};
};

std::vector<uint8_t> SerializePacket(const Packet& packet);
bool TryParsePacket(const std::vector<uint8_t>& bytes, Packet* packet);

Packet MakeHandshakePacket(const HandshakePacketData& handshake);
Packet MakeJoinRequestPacket(const JoinRequestPacketData& joinRequest);
Packet MakeJoinResponsePacket(const JoinResponsePacketData& joinResponse);
Packet MakeTopologySnapshotPacket(const TopologySnapshotPacketData& topologySnapshot);
Packet MakePlayerSnapshotPacket(const PlayerSnapshotPacketData& playerSnapshot);
Packet MakeChallengeRequestPacket(const ChallengeRequestPacketData& challengeRequest);
Packet MakeChallengeResponsePacket(const ChallengeResponsePacketData& challengeResponse);

bool TryDecodeHandshakePacket(const Packet& packet, HandshakePacketData* handshake);
bool TryDecodeJoinRequestPacket(const Packet& packet, JoinRequestPacketData* joinRequest);
bool TryDecodeJoinResponsePacket(const Packet& packet, JoinResponsePacketData* joinResponse, size_t maxCandidates = SIZE_MAX);
bool TryDecodeTopologySnapshotPacket(const Packet& packet, TopologySnapshotPacketData* topologySnapshot, size_t maxNodes = SIZE_MAX);
bool TryDecodePlayerSnapshotPacket(const Packet& packet, PlayerSnapshotPacketData* playerSnapshot);
bool TryDecodeChallengeRequestPacket(const Packet& packet, ChallengeRequestPacketData* challengeRequest);
bool TryDecodeChallengeResponsePacket(const Packet& packet, ChallengeResponsePacketData* challengeResponse);

bool BuildHandshakeSigningMessage(const HandshakePacketData& handshake, std::vector<uint8_t>* messageBytes);
bool BuildJoinRequestSigningMessage(const JoinRequestPacketData& joinRequest, std::vector<uint8_t>* messageBytes);
bool BuildJoinResponseSigningMessage(const JoinResponsePacketData& joinResponse, std::vector<uint8_t>* messageBytes);
bool BuildTopologySnapshotSigningMessage(const TopologySnapshotPacketData& topologySnapshot, std::vector<uint8_t>* messageBytes);
bool BuildPlayerSnapshotSigningMessage(const PlayerSnapshotPacketData& playerSnapshot, std::vector<uint8_t>* messageBytes);
bool BuildChallengeRequestSigningMessage(const ChallengeRequestPacketData& challengeRequest, std::vector<uint8_t>* messageBytes);
bool BuildChallengeResponseSigningMessage(const ChallengeResponsePacketData& challengeResponse, std::vector<uint8_t>* messageBytes);

std::string DescribePacketType(PacketType type);
