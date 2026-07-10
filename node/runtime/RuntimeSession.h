#pragma once

#include "ActiveNodeBucket.h"
#include "PacketValidator.h"
#include "RuntimeClient.h"

#include "../blockchain/Wallet.h"
#include "../world/World.h"

#include <cstdint>
#include <string>
#include <vector>

enum class RuntimeNodeRole : uint8_t
{
  Client = 0,
  Simulator = 1,
  Relay = 2,
};

struct RuntimeSessionConfig
{
  RuntimeNodeRole role = RuntimeNodeRole::Client;
  RuntimePeerAddress bindAddress = {"127.0.0.1", 46010};
  RuntimePeerAddress jumpNode = {};
  Wallet wallet = {};
  uint64_t realmHash = 0;
  RuntimeWorldPosition initialNodePosition = {};
  RuntimeInterestArea interestArea = {};
  RuntimeWorldPosition joinTargetPosition = {};
  uint32_t receiveTimeoutMs = 50;
  size_t maxNodeConnections = 8;
  size_t maxKnownNodes = 64;
  uint32_t maxJoinCandidates = 6;
  uint32_t maxJoinHops = 8;
  uint32_t neighborRefreshMs = 250;
  uint32_t topologyBroadcastMs = 500;
  uint32_t playerBroadcastMs = 50;
  bool enabled = true;
  bool acceptsJoins = true;
};

struct RuntimeSessionStatus
{
  bool running = false;
  bool joinResolved = false;
  RuntimeWorldPosition localNodePosition = {};
  RuntimeWorldPosition resolvedSpawnPosition = {};
  size_t knownNodes = 0;
  size_t connectedNodes = 0;
};

class RuntimeSession
{
 public:
  RuntimeSession() = default;
  ~RuntimeSession();

  bool Start(const RuntimeSessionConfig& sessionConfig);
  void Stop(World* world = nullptr);
  void Tick(uint32_t deltaMs, World* world = nullptr);

  bool IsRunning() const;
  RuntimeWorldPosition GetResolvedSpawnPosition() const;
  RuntimeSessionStatus GetStatus() const;
  void SetJoinTargetPosition(const RuntimeWorldPosition& position);
  void SetNodePosition(const RuntimeWorldPosition& position);

 private:
  struct RemotePlayerReplica
  {
    RuntimePeerAddress peerAddress = {};
    int playerId = 0;
    uint64_t lastSeenMs = 0;
    bool active = false;
  };

  struct PeerHint
  {
    TopologyNodeState node = {};
    RuntimePeerAddress introducedBy = {};
    uint64_t lastHintMs = 0;
  };

  void PumpIncomingPackets(World* world);
  void HandleParsedPacket(const Packet& packet, const RuntimePeerAddress& peerAddress, World* world);
  void HandleHandshake(const HandshakePacketData& handshake, const RuntimePeerAddress& peerAddress);
  void HandleChallengeRequest(const ChallengeRequestPacketData& challengeRequest, const RuntimePeerAddress& peerAddress);
  void HandleChallengeResponse(const ChallengeResponsePacketData& challengeResponse, const RuntimePeerAddress& peerAddress);
  void HandleJoinRequest(const JoinRequestPacketData& joinRequest, const RuntimePeerAddress& peerAddress);
  void HandleJoinResponse(const JoinResponsePacketData& joinResponse, const RuntimePeerAddress& peerAddress);
  void HandleTopologySnapshot(const TopologySnapshotPacketData& topologySnapshot, const RuntimePeerAddress& peerAddress);
  void HandlePlayerSnapshot(const PlayerSnapshotPacketData& playerSnapshot, const RuntimePeerAddress& peerAddress, World* world);
  void RefreshConnections();
  void BroadcastHandshake();
  void BroadcastTopology();
  void BroadcastLocalPlayer(World* world);
  void MaybeRequestJoin();
  void PruneStaleState(World* world);
  void ApplyRemotePlayerSnapshot(const RuntimePeerAddress& peerAddress, const PlayerSnapshotPacketData& playerSnapshot, World* world);
  int AcquireRemotePlayerId(const RuntimePeerAddress& peerAddress);
  void DespawnRemotePlayer(const RuntimePeerAddress& peerAddress, World* world);

  bool SignHandshakePacket(HandshakePacketData* handshake);
  bool SignJoinRequestPacket(JoinRequestPacketData* joinRequest);
  bool SignJoinResponsePacket(JoinResponsePacketData* joinResponse);
  bool SignTopologySnapshotPacket(TopologySnapshotPacketData* topologySnapshot);
  bool SignPlayerSnapshotPacket(PlayerSnapshotPacketData* playerSnapshot);
  bool SignChallengeRequestPacket(ChallengeRequestPacketData* challengeRequest);
  bool SignChallengeResponsePacket(ChallengeResponsePacketData* challengeResponse);

  HandshakePacketData BuildLocalHandshake();
  TopologyNodeState BuildLocalTopologyNode() const;
  PacketPeerProof BuildLocalProof();
  uint64_t NextJoinRequestToken();
  uint64_t NextChallengeNonce(const RuntimePeerAddress& peerAddress) const;
  bool AddPeerStrike(const RuntimePeerAddress& peerAddress, const std::string& reason, uint32_t suspicionDelta, uint32_t strikeDelta);
  bool IsPeerTrustedResponder(const RuntimePeerAddress& peerAddress) const;
  void RememberPeerHint(const TopologyNodeState& node, const RuntimePeerAddress& introducedBy);
  void SendHandshakeToHintedPeers();
  void MaybeSendChallengeRequest(const RuntimePeerAddress& peerAddress);
  bool IsAuthReady() const;

  void SendPacketTo(const RuntimePeerAddress& peerAddress, const Packet& packet);
  void SendPacketToConnectedPeers(const Packet& packet);

  RuntimeSessionConfig config = {};
  RuntimeClient client = {};
  ActiveNodeBucket knownNodes = ActiveNodeBucket(1);
  std::vector<RuntimePeerAddress> connectedPeerAddresses = {};
  std::vector<RemotePlayerReplica> remotePlayers = {};
  std::vector<PeerHint> peerHints = {};
  RuntimeWorldPosition localNodePosition = {};
  RuntimeWorldPosition resolvedSpawnPosition = {};
  std::string localSignerAddress = {};
  uint64_t localChallengeNonce = 0;
  uint64_t nowMs = 0;
  uint64_t localSessionId = 0;
  uint64_t lastNeighborRefreshMs = 0;
  uint64_t lastTopologyBroadcastMs = 0;
  uint64_t lastPlayerBroadcastMs = 0;
  uint64_t lastJoinRequestMs = 0;
  uint64_t joinRequestToken = 0;
  uint32_t nextPacketSequence = 1;
  bool joinResolved = false;
  bool running = false;
};
