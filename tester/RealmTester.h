#pragma once

#include "Args.h"
#include "../node/blockchain/RealmConfigFiles.h"
#include "../node/runtime/ActiveNodeBucket.h"
#include "../node/runtime/NodeConfigFiles.h"
#include "../node/runtime/PacketValidator.h"
#include "../node/runtime/RuntimeAuth.h"
#include "../node/runtime/RuntimeSession.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct TesterSummary
{
  bool success = false;
  bool sawHandshake = false;
  bool sawChallengeRequest = false;
  bool sawChallengeResponse = false;
  bool sawJoinResponse = false;
  bool sawTopologySnapshot = false;
  bool sawPlayerSnapshot = false;
  bool blockchainReady = false;
  size_t authenticatedPeers = 0;
  size_t maxTopologyNodes = 0;
  std::array<uint32_t, 8> receivedPacketCounts = {};
  std::array<uint32_t, 8> sentPacketCounts = {};
  std::string errorMessage = {};
};

class RealmTester
{
 public:
  explicit RealmTester(const TesterOptions& options);

  bool Run(TesterSummary* summary);

 private:
  struct PeerProbeState
  {
    RuntimePeerAddress peerAddress = {};
    uint64_t pendingChallengeNonce = 0;
    uint64_t lastChallengeRequestMs = 0;
    bool challengeRequestSent = false;
    bool authenticated = false;
    bool hinted = false;
  };

  bool LoadInputs(std::string* errorMessage);
  bool Start(std::string* errorMessage);
  void Stop();

  uint64_t BuildRealmHash();
  uint64_t NextSessionId() const;
  uint64_t NextChallengeNonce(const RuntimePeerAddress& peerAddress) const;
  uint64_t NextJoinRequestToken();
  uint64_t NowMs() const;
  size_t PacketIndex(PacketType type) const;
  PacketPeerProof BuildLocalProof();

  bool SignHandshakePacket(HandshakePacketData* handshake) const;
  bool SignChallengeRequestPacket(ChallengeRequestPacketData* challengeRequest) const;
  bool SignChallengeResponsePacket(ChallengeResponsePacketData* challengeResponse) const;
  bool SignJoinRequestPacket(JoinRequestPacketData* joinRequest) const;
  bool SignPlayerSnapshotPacket(PlayerSnapshotPacketData* playerSnapshot) const;

  bool SendPacketTo(const RuntimePeerAddress& peerAddress, const Packet& packet, PacketType type);
  void MaybeSendBootstrapHandshake(uint64_t nowMs);
  void MaybeSendHintedHandshakes(uint64_t nowMs);
  void MaybeSendJoinRequest(uint64_t nowMs);
  void MaybeSendPlayerSnapshot(uint64_t nowMs);
  bool PumpIncoming(uint64_t nowMs, std::string* errorMessage);
  bool HandleIncoming(const std::vector<uint8_t>& bytes, const RuntimePeerAddress& peerAddress, uint64_t nowMs, std::string* errorMessage);
  bool IsBootstrapNode(const RuntimePeerAddress& peerAddress) const;
  bool IsLocalPeerAddress(const RuntimePeerAddress& peerAddress) const;

  bool HandleValidatedHandshake(const PacketValidationResult& validation, const RuntimePeerAddress& peerAddress, uint64_t nowMs);
  bool HandleValidatedChallengeRequest(const PacketValidationResult& validation, const RuntimePeerAddress& peerAddress, uint64_t nowMs);
  bool HandleValidatedChallengeResponse(const PacketValidationResult& validation, const RuntimePeerAddress& peerAddress, std::string* errorMessage);
  bool HandleValidatedJoinResponse(const PacketValidationResult& validation);
  bool HandleValidatedTopologySnapshot(const PacketValidationResult& validation);
  bool HandleValidatedPlayerSnapshot(const PacketValidationResult& validation);

  PeerProbeState* FindPeerState(const RuntimePeerAddress& peerAddress);
  const PeerProbeState* FindPeerState(const RuntimePeerAddress& peerAddress) const;
  PeerProbeState* EnsurePeerState(const RuntimePeerAddress& peerAddress);
  void MarkPeerAuthenticated(const RuntimePeerAddress& peerAddress);
  bool AllExpectationsSatisfied() const;
  std::string BuildFailureMessage() const;

  TesterOptions options = {};
  NodeFilesConfig nodeFiles = {};
  RealmConfigFiles realmFiles = {};
  RuntimeClient client = {};
  ActiveNodeBucket activeNodes{64};
  RuntimePeerAddress jumpNode = {};
  std::vector<RuntimePeerAddress> bootstrapNodes = {};
  std::string localSignerAddress = {};
  std::vector<PeerProbeState> peerStates = {};
  std::array<uint32_t, 8> receivedPacketCounts = {};
  std::array<uint32_t, 8> sentPacketCounts = {};
  uint64_t localSessionId = 0;
  uint64_t localChallengeNonce = 0;
  uint64_t realmHash = 0;
  uint64_t lastBootstrapHandshakeMs = 0;
  uint64_t lastHintHandshakeMs = 0;
  uint64_t lastPlayerSnapshotMs = 0;
  uint64_t lastJoinRequestMs = 0;
  uint64_t activeJoinRequestToken = 0;
  uint32_t nextPacketSequence = 1;
  RuntimeNodeRole localRole = RuntimeNodeRole::Client;
  bool blockchainReady = false;
  bool sawHandshake = false;
  bool sawChallengeRequest = false;
  bool sawChallengeResponse = false;
  bool sawJoinResponse = false;
  bool sawTopologySnapshot = false;
  bool sawPlayerSnapshot = false;
  size_t maxTopologyNodes = 0;
};
