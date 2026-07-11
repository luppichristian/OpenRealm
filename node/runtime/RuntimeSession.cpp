#include "RuntimeSession.h"

#include "ProtocolVersion.h"
#include "RuntimeAuth.h"

#include "../client/PlayerController.h"

#include <algorithm>
#include <chrono>
#include <cmath>

constexpr uint64_t kJoinRetryIntervalMs = 500;
constexpr uint64_t kJoinResponseWindowMs = 4000;
constexpr uint64_t kStaleNodeMultiplier = 6;
constexpr uint64_t kStalePlayerMultiplier = 10;
constexpr uint64_t kPeerHintLifetimeMs = 15000;
constexpr uint64_t kChallengeRetryMs = 1000;
constexpr float kMaxPlayerSpeedPerSecond = 128.0f;
constexpr float kMaxPlayerTeleportDistance = 160.0f;
constexpr float kMaxPlayerNodeDriftDistance = 96.0f;

static uint64_t MixU64(uint64_t value)
{
  value ^= value >> 30;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31;
  return value;
}

static uint64_t HashPeerAddress(const RuntimePeerAddress& peerAddress)
{
  uint64_t hash = 1469598103934665603ULL;
  for (char ch : peerAddress.host)
  {
    hash ^= (uint8_t)ch;
    hash *= 1099511628211ULL;
  }
  hash ^= (uint64_t)std::max(peerAddress.port, 0);
  hash *= 1099511628211ULL;
  return hash == 0 ? 1ULL : hash;
}

static uint64_t GenerateRuntimeSessionId(const RuntimePeerAddress& peerAddress)
{
  const uint64_t timeSeed = (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
  const uint64_t mixed = MixU64(HashPeerAddress(peerAddress) ^ timeSeed ^ 0x9e3779b97f4a7c15ULL);
  return mixed == 0 ? 1ULL : mixed;
}

static bool IsFinitePosition(const RuntimeWorldPosition& position)
{
  return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

static bool IsFiniteInterestArea(const RuntimeInterestArea& area)
{
  return std::isfinite(area.radiusX) && std::isfinite(area.radiusY) && std::isfinite(area.radiusZ) && area.radiusX >= 0.0f && area.radiusY >= 0.0f && area.radiusZ >= 0.0f;
}

static bool IsValidPeerAddress(const RuntimePeerAddress& peerAddress)
{
  return !peerAddress.host.empty() && peerAddress.port > 0 && peerAddress.port <= 65535;
}

static bool IsHintExpired(uint64_t nowMs, uint64_t lastHintMs)
{
  return lastHintMs == 0 || nowMs - lastHintMs > kPeerHintLifetimeMs;
}

static void TouchKnownNode(ActiveNodeBucket* knownNodes, const RuntimePeerAddress& peerAddress, uint64_t tick)
{
  if (knownNodes == nullptr) return;
  ActiveNodeState* node = knownNodes->FindMutableByPeerAddress(peerAddress);
  if (node == nullptr) return;
  node->lastSeenTick = tick;
  node->acceptedPackets += 1;
}

RuntimeSession::~RuntimeSession()
{
  Stop();
}

bool RuntimeSession::IsAuthReady() const
{
  return !localSignerAddress.empty() && localChallengeNonce != 0 && config.wallet.HasPrivateKey();
}

bool RuntimeSession::SignHandshakePacket(HandshakePacketData* handshake)
{
  if (handshake == nullptr || !IsAuthReady())
  {
    TraceLog(LOG_WARNING, "runtime auth handshake signing skipped: auth_not_ready=%d", IsAuthReady() ? 0 : 1);
    return false;
  }
  handshake->signerAddress = localSignerAddress;
  std::vector<uint8_t> messageBytes = {};
  if (!BuildHandshakeSigningMessage(*handshake, &messageBytes))
  {
    TraceLog(LOG_WARNING, "runtime auth handshake signing failed: build_message");
    return false;
  }
  std::string signerAddress = {};
  std::string signatureHex = {};
  if (!SignRuntimeAuthMessage(config.wallet, messageBytes, &signerAddress, &signatureHex))
  {
    TraceLog(LOG_WARNING, "runtime auth handshake signing failed: sign_message");
    return false;
  }
  if (NormalizeRuntimeAuthAddress(signerAddress) != NormalizeRuntimeAuthAddress(localSignerAddress))
  {
    TraceLog(LOG_WARNING, "runtime auth handshake signing failed: signer_mismatch resolved=%s local=%s", signerAddress.c_str(), localSignerAddress.c_str());
    return false;
  }
  handshake->proof.signatureHex = signatureHex;
  return true;
}

bool RuntimeSession::SignJoinRequestPacket(JoinRequestPacketData* joinRequest)
{
  if (joinRequest == nullptr || !IsAuthReady()) return false;
  std::vector<uint8_t> messageBytes = {};
  if (!BuildJoinRequestSigningMessage(*joinRequest, &messageBytes)) return false;
  std::string signerAddress = {};
  std::string signatureHex = {};
  if (!SignRuntimeAuthMessage(config.wallet, messageBytes, &signerAddress, &signatureHex)) return false;
  if (NormalizeRuntimeAuthAddress(signerAddress) != NormalizeRuntimeAuthAddress(localSignerAddress)) return false;
  joinRequest->proof.signatureHex = signatureHex;
  return true;
}

bool RuntimeSession::SignJoinResponsePacket(JoinResponsePacketData* joinResponse)
{
  if (joinResponse == nullptr || !IsAuthReady()) return false;
  std::vector<uint8_t> messageBytes = {};
  if (!BuildJoinResponseSigningMessage(*joinResponse, &messageBytes)) return false;
  std::string signerAddress = {};
  std::string signatureHex = {};
  if (!SignRuntimeAuthMessage(config.wallet, messageBytes, &signerAddress, &signatureHex)) return false;
  if (NormalizeRuntimeAuthAddress(signerAddress) != NormalizeRuntimeAuthAddress(localSignerAddress)) return false;
  joinResponse->proof.signatureHex = signatureHex;
  return true;
}

bool RuntimeSession::SignTopologySnapshotPacket(TopologySnapshotPacketData* topologySnapshot)
{
  if (topologySnapshot == nullptr || !IsAuthReady()) return false;
  std::vector<uint8_t> messageBytes = {};
  if (!BuildTopologySnapshotSigningMessage(*topologySnapshot, &messageBytes)) return false;
  std::string signerAddress = {};
  std::string signatureHex = {};
  if (!SignRuntimeAuthMessage(config.wallet, messageBytes, &signerAddress, &signatureHex)) return false;
  if (NormalizeRuntimeAuthAddress(signerAddress) != NormalizeRuntimeAuthAddress(localSignerAddress)) return false;
  topologySnapshot->proof.signatureHex = signatureHex;
  return true;
}

bool RuntimeSession::SignPlayerSnapshotPacket(PlayerSnapshotPacketData* playerSnapshot)
{
  if (playerSnapshot == nullptr || !IsAuthReady()) return false;
  std::vector<uint8_t> messageBytes = {};
  if (!BuildPlayerSnapshotSigningMessage(*playerSnapshot, &messageBytes)) return false;
  std::string signerAddress = {};
  std::string signatureHex = {};
  if (!SignRuntimeAuthMessage(config.wallet, messageBytes, &signerAddress, &signatureHex)) return false;
  if (NormalizeRuntimeAuthAddress(signerAddress) != NormalizeRuntimeAuthAddress(localSignerAddress)) return false;
  playerSnapshot->proof.signatureHex = signatureHex;
  return true;
}

bool RuntimeSession::SignChallengeRequestPacket(ChallengeRequestPacketData* challengeRequest)
{
  if (challengeRequest == nullptr || !IsAuthReady()) return false;
  std::vector<uint8_t> messageBytes = {};
  if (!BuildChallengeRequestSigningMessage(*challengeRequest, &messageBytes)) return false;
  std::string signerAddress = {};
  std::string signatureHex = {};
  if (!SignRuntimeAuthMessage(config.wallet, messageBytes, &signerAddress, &signatureHex)) return false;
  if (NormalizeRuntimeAuthAddress(signerAddress) != NormalizeRuntimeAuthAddress(localSignerAddress)) return false;
  challengeRequest->proof.signatureHex = signatureHex;
  return true;
}

bool RuntimeSession::SignChallengeResponsePacket(ChallengeResponsePacketData* challengeResponse)
{
  if (challengeResponse == nullptr || !IsAuthReady()) return false;
  std::vector<uint8_t> messageBytes = {};
  if (!BuildChallengeResponseSigningMessage(*challengeResponse, &messageBytes)) return false;
  std::string signerAddress = {};
  std::string signatureHex = {};
  if (!SignRuntimeAuthMessage(config.wallet, messageBytes, &signerAddress, &signatureHex)) return false;
  if (NormalizeRuntimeAuthAddress(signerAddress) != NormalizeRuntimeAuthAddress(localSignerAddress)) return false;
  challengeResponse->proof.signatureHex = signatureHex;
  return true;
}

bool RuntimeSession::Start(const RuntimeSessionConfig& sessionConfig)
{
  Stop();
  config = sessionConfig;
  knownNodes = ActiveNodeBucket(config.maxKnownNodes);
  connectedPeerAddresses.clear();
  remotePlayers.clear();
  peerHints.clear();
  localNodePosition = config.initialNodePosition;
  resolvedSpawnPosition = config.joinTargetPosition;
  localSignerAddress.clear();
  localChallengeNonce = 0;
  nowMs = 0;
  localSessionId = GenerateRuntimeSessionId(config.bindAddress);
  lastNeighborRefreshMs = 0;
  lastTopologyBroadcastMs = 0;
  lastPlayerBroadcastMs = 0;
  lastJoinRequestMs = 0;
  joinRequestToken = 0;
  nextPacketSequence = 1;
  joinResolved = !config.enabled || config.jumpNode.host.empty() || config.jumpNode.port <= 0;
  if (joinResolved)
  {
    resolvedSpawnPosition = localNodePosition;
  }

  if (!config.enabled)
  {
    running = true;
    return true;
  }

  if (!config.wallet.HasPrivateKey())
  {
    TraceLog(LOG_ERROR, "Runtime auth requires a wallet private key.");
    return false;
  }
  if (!ResolveRuntimeWalletAddress(config.wallet, &localSignerAddress))
  {
    TraceLog(LOG_ERROR, "Failed to resolve runtime wallet address for authenticated runtime session.");
    return false;
  }
  localChallengeNonce = NextChallengeNonce(config.bindAddress);

  if (!client.Start(config.bindAddress))
  {
    return false;
  }

  running = true;
  if (!config.jumpNode.host.empty() && config.jumpNode.port > 0)
  {
    HandshakePacketData handshake = BuildLocalHandshake();
    if (SignHandshakePacket(&handshake))
    {
      SendPacketTo(config.jumpNode, MakeHandshakePacket(handshake));
    }
  }
  return true;
}

void RuntimeSession::Stop(World* world)
{
  if (world != nullptr)
  {
    for (const RemotePlayerReplica& replica : remotePlayers)
    {
      if (!replica.active) continue;
      WorldEvent event = {.type = WorldEventType::Despawn, .playerId = replica.playerId};
      world->SendEvent(event);
    }
  }

  remotePlayers.clear();
  peerHints.clear();
  connectedPeerAddresses.clear();
  knownNodes = ActiveNodeBucket(1);
  client.Stop();
  running = false;
  joinResolved = false;
  localSignerAddress.clear();
  localChallengeNonce = 0;
  nowMs = 0;
  localSessionId = 0;
  joinRequestToken = 0;
  nextPacketSequence = 1;
}

bool RuntimeSession::IsRunning() const
{
  return running;
}

RuntimeWorldPosition RuntimeSession::GetResolvedSpawnPosition() const
{
  return resolvedSpawnPosition;
}

RuntimeSessionStatus RuntimeSession::GetStatus() const
{
  return {
      .running = running,
      .joinResolved = joinResolved,
      .localNodePosition = localNodePosition,
      .resolvedSpawnPosition = resolvedSpawnPosition,
      .knownNodes = knownNodes.GetCount(),
      .connectedNodes = connectedPeerAddresses.size(),
  };
}

void RuntimeSession::SetJoinTargetPosition(const RuntimeWorldPosition& position)
{
  config.joinTargetPosition = position;
  if (!joinResolved) resolvedSpawnPosition = position;
}

void RuntimeSession::SetNodePosition(const RuntimeWorldPosition& position)
{
  localNodePosition = position;
}

void RuntimeSession::Tick(uint32_t deltaMs, World* world)
{
  if (!running) return;
  nowMs += deltaMs;

  PumpIncomingPackets(world);
  if (!config.enabled) return;

  if (!joinResolved) MaybeRequestJoin();

  if (nowMs - lastNeighborRefreshMs >= config.neighborRefreshMs)
  {
    RefreshConnections();
    lastNeighborRefreshMs = nowMs;
  }

  if (nowMs - lastTopologyBroadcastMs >= config.topologyBroadcastMs)
  {
    BroadcastHandshake();
    BroadcastTopology();
    lastTopologyBroadcastMs = nowMs;
  }

  if (world != nullptr && nowMs - lastPlayerBroadcastMs >= config.playerBroadcastMs)
  {
    BroadcastLocalPlayer(world);
    lastPlayerBroadcastMs = nowMs;
  }

  PruneStaleState(world);
}

void RuntimeSession::PumpIncomingPackets(World* world)
{
  if (!config.enabled) return;

  while (true)
  {
    std::vector<uint8_t> bytes = {};
    RuntimePeerAddress peerAddress = {};
    if (!client.ReceivePacket(&bytes, &peerAddress, 0)) break;

    PacketValidationContext validationContext = {};
    validationContext.localPeerAddress = config.bindAddress;
    validationContext.bootstrapPeerAddress = config.jumpNode;
    validationContext.expectedProtocolVersion = kRuntimeProtocolVersion;
    validationContext.expectedRealmHash = config.realmHash;
    validationContext.activeNodes = &knownNodes;
    validationContext.tick = nowMs;
    validationContext.maxKnownNodes = config.maxKnownNodes;
    validationContext.maxJoinCandidates = config.maxJoinCandidates;
    validationContext.maxJoinHops = config.maxJoinHops;

    PacketValidationResult validation = ValidateIncomingPacket(bytes, peerAddress, validationContext);
    if (!validation.accepted)
    {
      if (validation.code != PacketValidationCode::SelfPeerAddress)
      {
        TraceLog(
            LOG_WARNING,
            "runtime dropped %s packet from %s: %s",
            DescribePacketType(validation.packet.type).c_str(),
            DescribeRuntimePeerAddress(peerAddress).c_str(),
            DescribePacketValidationCode(validation.code).c_str());
      }

      switch (validation.code)
      {
        case PacketValidationCode::InvalidSession:
          AddPeerStrike(peerAddress, "invalid_session", 3, 2);
          break;
        case PacketValidationCode::InvalidSequence:
          AddPeerStrike(peerAddress, "replayed_or_out_of_order_packet", 2, 1);
          break;
        case PacketValidationCode::InvalidSignature:
          AddPeerStrike(peerAddress, "invalid_signature", 4, 2);
          break;
        case PacketValidationCode::MissingSignerAddress:
        case PacketValidationCode::UnauthenticatedPeer:
        case PacketValidationCode::QuarantinedPeer:
        case PacketValidationCode::BannedPeer:
          break;
        case PacketValidationCode::InvalidJoinRequestPayload:
        case PacketValidationCode::InvalidJoinResponsePayload:
        case PacketValidationCode::InvalidTopologySnapshotPayload:
        case PacketValidationCode::InvalidPlayerSnapshotPayload:
        case PacketValidationCode::InvalidChallengeRequestPayload:
        case PacketValidationCode::InvalidChallengeResponsePayload:
          AddPeerStrike(peerAddress, DescribePacketValidationCode(validation.code), 2, 1);
          break;
        default:
          break;
      }
      continue;
    }

    HandleParsedPacket(validation.packet, peerAddress, world);
  }
}

void RuntimeSession::HandleParsedPacket(const Packet& packet, const RuntimePeerAddress& peerAddress, World* world)
{
  switch (packet.type)
  {
    case PacketType::Handshake:
    {
      HandshakePacketData handshake = {};
      if (!TryDecodeHandshakePacket(packet, &handshake)) return;
      HandleHandshake(handshake, peerAddress);
    }
    break;

    case PacketType::ChallengeRequest:
    {
      ChallengeRequestPacketData challengeRequest = {};
      if (!TryDecodeChallengeRequestPacket(packet, &challengeRequest)) return;
      HandleChallengeRequest(challengeRequest, peerAddress);
    }
    break;

    case PacketType::ChallengeResponse:
    {
      ChallengeResponsePacketData challengeResponse = {};
      if (!TryDecodeChallengeResponsePacket(packet, &challengeResponse)) return;
      HandleChallengeResponse(challengeResponse, peerAddress);
    }
    break;

    case PacketType::JoinRequest:
    {
      JoinRequestPacketData joinRequest = {};
      if (!TryDecodeJoinRequestPacket(packet, &joinRequest)) return;
      HandleJoinRequest(joinRequest, peerAddress);
    }
    break;

    case PacketType::JoinResponse:
    {
      JoinResponsePacketData joinResponse = {};
      if (!TryDecodeJoinResponsePacket(packet, &joinResponse, config.maxKnownNodes)) return;
      HandleJoinResponse(joinResponse, peerAddress);
    }
    break;

    case PacketType::TopologySnapshot:
    {
      TopologySnapshotPacketData topologySnapshot = {};
      if (!TryDecodeTopologySnapshotPacket(packet, &topologySnapshot, config.maxKnownNodes)) return;
      HandleTopologySnapshot(topologySnapshot, peerAddress);
    }
    break;

    case PacketType::PlayerSnapshot:
    {
      PlayerSnapshotPacketData playerSnapshot = {};
      if (!TryDecodePlayerSnapshotPacket(packet, &playerSnapshot)) return;
      HandlePlayerSnapshot(playerSnapshot, peerAddress, world);
    }
    break;

    default:
      TraceLog(LOG_WARNING, "runtime received unsupported packet type %s from %s", DescribePacketType(packet.type).c_str(), DescribeRuntimePeerAddress(peerAddress).c_str());
      break;
  }
}

void RuntimeSession::HandleHandshake(const HandshakePacketData& handshake, const RuntimePeerAddress& peerAddress)
{
  TouchKnownNode(&knownNodes, peerAddress, nowMs);

  HandshakePacketData localHandshake = BuildLocalHandshake();
  if (SignHandshakePacket(&localHandshake))
  {
    SendPacketTo(peerAddress, MakeHandshakePacket(localHandshake));
  }

  ActiveNodeState* node = knownNodes.FindMutableByPeerAddress(peerAddress);
  if (node == nullptr) return;
  node->signerAddress = handshake.signerAddress;
  node->advertisedChallengeNonce = handshake.challengeNonce;
  MaybeSendChallengeRequest(peerAddress);
}

void RuntimeSession::MaybeSendChallengeRequest(const RuntimePeerAddress& peerAddress)
{
  ActiveNodeState* node = knownNodes.FindMutableByPeerAddress(peerAddress);
  if (node == nullptr || node->authenticated || knownNodes.IsPeerIsolated(peerAddress, nowMs)) return;
  if (node->signerAddress.empty()) return;
  if (node->pendingAuthChallengeNonce != 0 && nowMs - node->pendingAuthChallengeTick < kChallengeRetryMs) return;

  node->pendingAuthChallengeNonce = NextChallengeNonce(peerAddress);
  node->pendingAuthChallengeTick = nowMs;

  ChallengeRequestPacketData challengeRequest = {};
  challengeRequest.proof = BuildLocalProof();
  challengeRequest.challengeNonce = node->pendingAuthChallengeNonce;
  if (!SignChallengeRequestPacket(&challengeRequest)) return;
  SendPacketTo(peerAddress, MakeChallengeRequestPacket(challengeRequest));
}

void RuntimeSession::HandleChallengeRequest(const ChallengeRequestPacketData& challengeRequest, const RuntimePeerAddress& peerAddress)
{
  ActiveNodeState* node = knownNodes.FindMutableByPeerAddress(peerAddress);
  if (node == nullptr || knownNodes.IsPeerIsolated(peerAddress, nowMs)) return;

  ChallengeResponsePacketData challengeResponse = {};
  challengeResponse.proof = BuildLocalProof();
  challengeResponse.challengeNonce = challengeRequest.challengeNonce;
  if (!SignChallengeResponsePacket(&challengeResponse)) return;
  SendPacketTo(peerAddress, MakeChallengeResponsePacket(challengeResponse));
  MaybeSendChallengeRequest(peerAddress);
}

void RuntimeSession::HandleChallengeResponse(const ChallengeResponsePacketData& challengeResponse, const RuntimePeerAddress& peerAddress)
{
  ActiveNodeState* node = knownNodes.FindMutableByPeerAddress(peerAddress);
  if (node == nullptr || knownNodes.IsPeerIsolated(peerAddress, nowMs)) return;
  if (node->pendingAuthChallengeNonce == 0 || node->pendingAuthChallengeNonce != challengeResponse.challengeNonce)
  {
    AddPeerStrike(peerAddress, "challenge_response_mismatch", 3, 2);
    return;
  }
  node->pendingAuthChallengeNonce = 0;
  node->pendingAuthChallengeTick = 0;
  node->authenticated = true;
  TraceLog(LOG_INFO, "runtime authenticated peer %s", DescribeRuntimePeerAddress(peerAddress).c_str());
  TouchKnownNode(&knownNodes, peerAddress, nowMs);
}

void RuntimeSession::HandleJoinRequest(const JoinRequestPacketData& joinRequest, const RuntimePeerAddress& peerAddress)
{
  if (!config.acceptsJoins) return;
  if (knownNodes.IsPeerIsolated(peerAddress, nowMs) || !knownNodes.IsPeerAuthenticated(peerAddress)) return;
  if (knownNodes.FindByPeerAddress(peerAddress) == nullptr) return;

  TraceLog(
      LOG_INFO,
      "runtime join request from %s token=%llu target=(%.1f, %.1f, %.1f)",
      DescribeRuntimePeerAddress(peerAddress).c_str(),
      (unsigned long long)joinRequest.requestToken,
      (double)joinRequest.targetPosition.x,
      (double)joinRequest.targetPosition.y,
      (double)joinRequest.targetPosition.z);

  std::vector<const ActiveNodeState*> closest = knownNodes.BuildClosestNodes(
      peerAddress,
      config.realmHash,
      joinRequest.targetPosition,
      std::min((size_t)joinRequest.maxCandidates, config.maxKnownNodes),
      true,
      nowMs);

  JoinResponsePacketData response = {};
  response.proof = BuildLocalProof();
  response.requestToken = joinRequest.requestToken;
  response.resolvedPosition = localNodePosition;

  const float localDistance = ComputeRuntimeWorldDistanceSquared(localNodePosition, joinRequest.targetPosition);
  float resolvedDistance = localDistance;
  response.candidates.push_back(BuildLocalTopologyNode());

  for (const ActiveNodeState* node : closest)
  {
    if (node == nullptr) continue;
    response.candidates.push_back(ToTopologyNodeState(*node));
    const float candidateDistance = ComputeRuntimeWorldDistanceSquared(node->position, joinRequest.targetPosition);
    if (candidateDistance < resolvedDistance)
    {
      resolvedDistance = candidateDistance;
      response.resolvedPosition = node->position;
    }
  }

  if (!SignJoinResponsePacket(&response)) return;
  TouchKnownNode(&knownNodes, peerAddress, nowMs);
  TraceLog(LOG_INFO, "runtime join response to %s candidates=%d", DescribeRuntimePeerAddress(peerAddress).c_str(), (int)response.candidates.size());
  SendPacketTo(peerAddress, MakeJoinResponsePacket(response));

  TopologySnapshotPacketData snapshot = {};
  snapshot.proof = BuildLocalProof();
  snapshot.nodes.push_back(BuildLocalTopologyNode());
  std::vector<TopologyNodeState> knownSnapshot = knownNodes.BuildTopologySnapshot(
      config.bindAddress,
      config.realmHash,
      config.maxKnownNodes,
      nowMs);
  snapshot.nodes.insert(snapshot.nodes.end(), knownSnapshot.begin(), knownSnapshot.end());
  if (!SignTopologySnapshotPacket(&snapshot)) return;
  TraceLog(LOG_INFO, "runtime immediate topology to %s nodes=%d", DescribeRuntimePeerAddress(peerAddress).c_str(), (int)snapshot.nodes.size());
  SendPacketTo(peerAddress, MakeTopologySnapshotPacket(snapshot));
}

void RuntimeSession::HandleJoinResponse(const JoinResponsePacketData& joinResponse, const RuntimePeerAddress& peerAddress)
{
  if (!IsPeerTrustedResponder(peerAddress))
  {
    AddPeerStrike(peerAddress, "untrusted_join_responder", 2, 1);
    return;
  }
  if (joinRequestToken == 0 || joinResponse.requestToken != joinRequestToken)
  {
    AddPeerStrike(peerAddress, "join_response_request_token_mismatch", 2, 1);
    return;
  }
  if (nowMs - lastJoinRequestMs > kJoinResponseWindowMs)
  {
    AddPeerStrike(peerAddress, "join_response_outside_request_window", 1, 1);
    return;
  }

  bool sawResponderSelf = false;
  float bestDistance = ComputeRuntimeWorldDistanceSquared(joinResponse.resolvedPosition, config.joinTargetPosition);
  RuntimeWorldPosition bestPosition = joinResponse.resolvedPosition;

  for (const TopologyNodeState& node : joinResponse.candidates)
  {
    if (!IsValidPeerAddress(node.peerAddress) || !IsFinitePosition(node.position) || !IsFiniteInterestArea(node.interestArea))
    {
      AddPeerStrike(peerAddress, "join_response_invalid_candidate_shape", 2, 1);
      continue;
    }
    if (RuntimePeerAddressEquals(node.peerAddress, config.bindAddress)) continue;

    const float candidateDistance = ComputeRuntimeWorldDistanceSquared(node.position, config.joinTargetPosition);
    if (candidateDistance < bestDistance)
    {
      bestDistance = candidateDistance;
      bestPosition = node.position;
    }

    if (RuntimePeerAddressEquals(node.peerAddress, peerAddress))
    {
      if (node.sessionId != joinResponse.proof.sessionId)
      {
        AddPeerStrike(peerAddress, "join_response_self_entry_session_mismatch", 3, 2);
        continue;
      }
      sawResponderSelf = true;
      knownNodes.RegisterTopologyNode(node, 0, nowMs);
      continue;
    }

    const ActiveNodeState* knownNode = knownNodes.FindByPeerAddress(node.peerAddress);
    if (knownNode != nullptr && knownNode->sessionId != 0 && knownNode->sessionId != node.sessionId)
    {
      AddPeerStrike(peerAddress, "join_response_third_party_session_mismatch", 2, 1);
      continue;
    }

    RememberPeerHint(node, peerAddress);
  }

  if (!sawResponderSelf)
  {
    AddPeerStrike(peerAddress, "join_response_missing_self_entry", 1, 1);
  }

  resolvedSpawnPosition = bestPosition;
  localNodePosition = bestPosition;
  joinResolved = true;
  joinRequestToken = 0;
  TouchKnownNode(&knownNodes, peerAddress, nowMs);
  SendHandshakeToHintedPeers();
}

void RuntimeSession::HandleTopologySnapshot(const TopologySnapshotPacketData& topologySnapshot, const RuntimePeerAddress& peerAddress)
{
  if (!IsPeerTrustedResponder(peerAddress))
  {
    AddPeerStrike(peerAddress, "untrusted_topology_sender", 2, 1);
    return;
  }

  bool sawSenderSelf = false;
  for (const TopologyNodeState& node : topologySnapshot.nodes)
  {
    if (!IsValidPeerAddress(node.peerAddress) || !IsFinitePosition(node.position) || !IsFiniteInterestArea(node.interestArea))
    {
      AddPeerStrike(peerAddress, "topology_invalid_node_shape", 2, 1);
      continue;
    }
    if (RuntimePeerAddressEquals(node.peerAddress, config.bindAddress)) continue;

    if (RuntimePeerAddressEquals(node.peerAddress, peerAddress))
    {
      if (node.sessionId != topologySnapshot.proof.sessionId)
      {
        AddPeerStrike(peerAddress, "topology_self_entry_session_mismatch", 3, 2);
        continue;
      }
      sawSenderSelf = true;
      knownNodes.RegisterTopologyNode(node, 0, nowMs);
      continue;
    }

    const ActiveNodeState* knownNode = knownNodes.FindByPeerAddress(node.peerAddress);
    if (knownNode != nullptr && knownNode->sessionId != 0 && knownNode->sessionId != node.sessionId)
    {
      AddPeerStrike(peerAddress, "topology_third_party_session_mismatch", 2, 1);
      continue;
    }

    RememberPeerHint(node, peerAddress);
  }

  if (!sawSenderSelf)
  {
    AddPeerStrike(peerAddress, "topology_missing_self_entry", 1, 1);
  }
  TouchKnownNode(&knownNodes, peerAddress, nowMs);
}

void RuntimeSession::HandlePlayerSnapshot(const PlayerSnapshotPacketData& playerSnapshot, const RuntimePeerAddress& peerAddress, World* world)
{
  if (RuntimePeerAddressEquals(peerAddress, config.bindAddress)) return;
  if (knownNodes.IsPeerIsolated(peerAddress, nowMs) || !knownNodes.IsPeerAuthenticated(peerAddress)) return;

  ActiveNodeState* node = knownNodes.FindMutableByPeerAddress(peerAddress);
  if (node == nullptr) return;

  const RuntimeWorldPosition previousPosition = node->position;
  const uint64_t previousTick = node->lastSeenTick;
  const uint64_t deltaMs = previousTick > 0 && nowMs > previousTick ? nowMs - previousTick : 0;

  if (deltaMs > 0)
  {
    const float maxDistance = std::max(kMaxPlayerTeleportDistance, kMaxPlayerSpeedPerSecond * (float)deltaMs / 1000.0f);
    const float movementDistanceSquared = ComputeRuntimeWorldDistanceSquared(previousPosition, playerSnapshot.nodePosition);
    if (movementDistanceSquared > maxDistance * maxDistance)
    {
      AddPeerStrike(peerAddress, "player_snapshot_impossible_node_movement", 3, 2);
      return;
    }
  }

  const float playerNodeDriftSquared = ComputeRuntimeWorldDistanceSquared(playerSnapshot.nodePosition, playerSnapshot.playerPosition);
  if (playerNodeDriftSquared > kMaxPlayerNodeDriftDistance * kMaxPlayerNodeDriftDistance)
  {
    AddPeerStrike(peerAddress, "player_snapshot_position_drift", 2, 1);
    return;
  }

  if (!RuntimeInterestAreasOverlap(localNodePosition, config.interestArea, playerSnapshot.nodePosition, node->interestArea))
  {
    AddPeerStrike(peerAddress, "player_snapshot_outside_interest_area", 1, 1);
    return;
  }

  node->position = playerSnapshot.nodePosition;
  node->lastSeenTick = nowMs;
  node->acceptedPackets += 1;
  if (world == nullptr) return;
  ApplyRemotePlayerSnapshot(peerAddress, playerSnapshot, world);
}

void RuntimeSession::RefreshConnections()
{
  std::vector<const ActiveNodeState*> candidates = knownNodes.BuildNeighborCandidates(
      config.bindAddress,
      config.realmHash,
      localNodePosition,
      config.interestArea,
      config.maxNodeConnections,
      nowMs);
  if (candidates.empty())
  {
    candidates = knownNodes.BuildClosestNodes(
        config.bindAddress,
        config.realmHash,
        localNodePosition,
        std::min((size_t)1, config.maxNodeConnections),
        false,
        nowMs);
  }

  for (const ActiveNodeState& node : knownNodes.GetNodes())
  {
    knownNodes.MarkConnected(node.peerAddress, false);
    if (!node.authenticated)
    {
      MaybeSendChallengeRequest(node.peerAddress);
    }
  }

  connectedPeerAddresses.clear();
  for (const ActiveNodeState* candidate : candidates)
  {
    if (candidate == nullptr || !candidate->authenticated) continue;
    connectedPeerAddresses.push_back(candidate->peerAddress);
    knownNodes.MarkConnected(candidate->peerAddress, true);

    HandshakePacketData handshake = BuildLocalHandshake();
    if (SignHandshakePacket(&handshake))
    {
      SendPacketTo(candidate->peerAddress, MakeHandshakePacket(handshake));
    }
  }

  SendHandshakeToHintedPeers();
}

void RuntimeSession::BroadcastHandshake()
{
  HandshakePacketData handshake = BuildLocalHandshake();
  if (!SignHandshakePacket(&handshake)) return;
  SendPacketToConnectedPeers(MakeHandshakePacket(handshake));
  if (!config.jumpNode.host.empty() && config.jumpNode.port > 0)
  {
    SendPacketTo(config.jumpNode, MakeHandshakePacket(handshake));
  }
}

void RuntimeSession::BroadcastTopology()
{
  TopologySnapshotPacketData snapshot = {};
  snapshot.proof = BuildLocalProof();
  snapshot.nodes.push_back(BuildLocalTopologyNode());

  std::vector<TopologyNodeState> knownSnapshot = knownNodes.BuildTopologySnapshot(
      config.bindAddress,
      config.realmHash,
      config.maxKnownNodes,
      nowMs);
  snapshot.nodes.insert(snapshot.nodes.end(), knownSnapshot.begin(), knownSnapshot.end());
  if (!SignTopologySnapshotPacket(&snapshot)) return;
  SendPacketToConnectedPeers(MakeTopologySnapshotPacket(snapshot));
}

void RuntimeSession::BroadcastLocalPlayer(World* world)
{
  if (world == nullptr) return;
  const PlayerState* localPlayer = world->GetPlayerSystem().FindPlayer(LOCAL_PLAYER_ID);
  if (localPlayer == nullptr) return;

  localNodePosition = {
      .x = localPlayer->position.x,
      .y = localPlayer->position.y,
      .z = localPlayer->position.z,
  };

  PlayerSnapshotPacketData snapshot = {};
  snapshot.proof = BuildLocalProof();
  snapshot.tickMs = (uint32_t)std::min<uint64_t>(nowMs, UINT32_MAX);
  snapshot.nodePosition = localNodePosition;
  snapshot.playerPosition = localNodePosition;
  snapshot.yaw = localPlayer->yaw;
  snapshot.pitch = localPlayer->pitch;
  snapshot.active = 1;
  if (!SignPlayerSnapshotPacket(&snapshot)) return;
  SendPacketToConnectedPeers(MakePlayerSnapshotPacket(snapshot));
}

void RuntimeSession::MaybeRequestJoin()
{
  if (joinResolved) return;
  if (nowMs - lastJoinRequestMs < kJoinRetryIntervalMs) return;

  JoinRequestPacketData request = {};
  request.proof = BuildLocalProof();
  request.targetPosition = config.joinTargetPosition;
  request.maxCandidates = config.maxJoinCandidates;
  request.maxHops = config.maxJoinHops;
  request.requestToken = NextJoinRequestToken();
  if (!SignJoinRequestPacket(&request)) return;
  const Packet packet = MakeJoinRequestPacket(request);

  bool sent = false;
  if (!config.jumpNode.host.empty() && config.jumpNode.port > 0 && knownNodes.IsPeerAuthenticated(config.jumpNode))
  {
    SendPacketTo(config.jumpNode, packet);
    sent = true;
  }
  for (const RuntimePeerAddress& peerAddress : connectedPeerAddresses)
  {
    const ActiveNodeState* node = knownNodes.FindByPeerAddress(peerAddress);
    if (node == nullptr || !node->authenticated || !node->acceptsJoins || knownNodes.IsPeerIsolated(node->peerAddress, nowMs)) continue;
    SendPacketTo(node->peerAddress, packet);
    sent = true;
  }
  for (const PeerHint& hint : peerHints)
  {
    if (IsHintExpired(nowMs, hint.lastHintMs) || !hint.node.acceptsJoins) continue;
    const ActiveNodeState* knownNode = knownNodes.FindByPeerAddress(hint.node.peerAddress);
    if (knownNode != nullptr)
    {
      if (knownNode->authenticated && !knownNodes.IsPeerIsolated(hint.node.peerAddress, nowMs))
      {
        SendPacketTo(hint.node.peerAddress, packet);
        sent = true;
      }
      continue;
    }

    HandshakePacketData handshake = BuildLocalHandshake();
    if (SignHandshakePacket(&handshake))
    {
      SendPacketTo(hint.node.peerAddress, MakeHandshakePacket(handshake));
    }
  }

  if (sent)
  {
    joinRequestToken = request.requestToken;
    lastJoinRequestMs = nowMs;
  }
}

void RuntimeSession::PruneStaleState(World* world)
{
  knownNodes.ClearExpiredIsolation(nowMs);

  const uint64_t staleNodeCutoff = nowMs > config.topologyBroadcastMs * kStaleNodeMultiplier
                                       ? nowMs - config.topologyBroadcastMs * kStaleNodeMultiplier
                                       : 0;
  knownNodes.ForgetStaleNodes(staleNodeCutoff, nowMs);

  connectedPeerAddresses.erase(
      std::remove_if(connectedPeerAddresses.begin(), connectedPeerAddresses.end(), [&](const RuntimePeerAddress& peerAddress)
                     {
                       return knownNodes.FindByPeerAddress(peerAddress) == nullptr || knownNodes.IsPeerIsolated(peerAddress, nowMs) || !knownNodes.IsPeerAuthenticated(peerAddress);
                     }),
      connectedPeerAddresses.end());

  peerHints.erase(
      std::remove_if(peerHints.begin(), peerHints.end(), [&](const PeerHint& hint)
                     {
                       if (IsHintExpired(nowMs, hint.lastHintMs)) return true;
                       const ActiveNodeState* knownNode = knownNodes.FindByPeerAddress(hint.node.peerAddress);
                       return knownNode != nullptr && knownNode->sessionId == hint.node.sessionId && knownNode->authenticated;
                     }),
      peerHints.end());

  const uint64_t stalePlayerCutoff = nowMs > config.playerBroadcastMs * kStalePlayerMultiplier
                                         ? nowMs - config.playerBroadcastMs * kStalePlayerMultiplier
                                         : 0;
  for (RemotePlayerReplica& replica : remotePlayers)
  {
    const bool peerMissing = knownNodes.FindByPeerAddress(replica.peerAddress) == nullptr;
    const bool peerIsolated = knownNodes.IsPeerIsolated(replica.peerAddress, nowMs);
    const bool peerUnauthed = !knownNodes.IsPeerAuthenticated(replica.peerAddress);
    if (!replica.active || (!peerMissing && !peerIsolated && !peerUnauthed && replica.lastSeenMs >= stalePlayerCutoff)) continue;
    if (world != nullptr)
    {
      WorldEvent event = {.type = WorldEventType::Despawn, .playerId = replica.playerId};
      world->SendEvent(event);
    }
    replica = {};
  }
}

void RuntimeSession::ApplyRemotePlayerSnapshot(const RuntimePeerAddress& peerAddress, const PlayerSnapshotPacketData& playerSnapshot, World* world)
{
  if (world == nullptr) return;

  const int playerId = AcquireRemotePlayerId(peerAddress);
  if (playerId < 0) return;

  WorldEvent event = {};
  event.type = WorldEventType::Spawn;
  event.playerId = playerId;
  event.playerX = playerSnapshot.playerPosition.x;
  event.playerY = playerSnapshot.playerPosition.y;
  event.playerZ = playerSnapshot.playerPosition.z;
  event.playerYaw = playerSnapshot.yaw;
  event.playerPitch = playerSnapshot.pitch;
  world->SendEvent(event);

  for (RemotePlayerReplica& replica : remotePlayers)
  {
    if (!replica.active || !RuntimePeerAddressEquals(replica.peerAddress, peerAddress)) continue;
    replica.lastSeenMs = nowMs;
    return;
  }
}

int RuntimeSession::AcquireRemotePlayerId(const RuntimePeerAddress& peerAddress)
{
  for (const RemotePlayerReplica& replica : remotePlayers)
  {
    if (!replica.active || !RuntimePeerAddressEquals(replica.peerAddress, peerAddress)) continue;
    return replica.playerId;
  }

  int nextId = 2;
  while (true)
  {
    bool used = false;
    for (const RemotePlayerReplica& replica : remotePlayers)
    {
      if (replica.active && replica.playerId == nextId)
      {
        used = true;
        break;
      }
    }
    if (!used) break;
    ++nextId;
    if (nextId > 4096) return -1;
  }

  for (RemotePlayerReplica& replica : remotePlayers)
  {
    if (replica.active) continue;
    replica.peerAddress = peerAddress;
    replica.playerId = nextId;
    replica.lastSeenMs = nowMs;
    replica.active = true;
    return replica.playerId;
  }

  remotePlayers.push_back({
      .peerAddress = peerAddress,
      .playerId = nextId,
      .lastSeenMs = nowMs,
      .active = true,
  });
  return nextId;
}

void RuntimeSession::DespawnRemotePlayer(const RuntimePeerAddress& peerAddress, World* world)
{
  if (world == nullptr) return;
  for (RemotePlayerReplica& replica : remotePlayers)
  {
    if (!replica.active || !RuntimePeerAddressEquals(replica.peerAddress, peerAddress)) continue;
    WorldEvent event = {.type = WorldEventType::Despawn, .playerId = replica.playerId};
    world->SendEvent(event);
    replica = {};
    return;
  }
}

HandshakePacketData RuntimeSession::BuildLocalHandshake()
{
  return {
      .proof = BuildLocalProof(),
      .protocolVersion = kRuntimeProtocolVersion,
      .realmHash = config.realmHash,
      .position = localNodePosition,
      .interestArea = config.interestArea,
      .nodeRole = (uint8_t)config.role,
      .acceptsJoins = (uint8_t)(config.acceptsJoins ? 1 : 0),
      .reserved = 0,
      .challengeNonce = localChallengeNonce,
      .signerAddress = localSignerAddress,
  };
}

TopologyNodeState RuntimeSession::BuildLocalTopologyNode() const
{
  return {
      .sessionId = localSessionId,
      .protocolVersion = kRuntimeProtocolVersion,
      .realmHash = config.realmHash,
      .peerAddress = client.GetBindAddress(),
      .position = localNodePosition,
      .interestArea = config.interestArea,
      .nodeRole = (uint8_t)config.role,
      .acceptsJoins = (uint8_t)(config.acceptsJoins ? 1 : 0),
  };
}

PacketPeerProof RuntimeSession::BuildLocalProof()
{
  return {
      .sessionId = localSessionId,
      .sequence = nextPacketSequence++,
      .signatureHex = {},
  };
}

uint64_t RuntimeSession::NextJoinRequestToken()
{
  uint64_t token = MixU64(localSessionId ^ (uint64_t)nextPacketSequence ^ (nowMs << 1) ^ 0x517cc1b727220a95ULL);
  return token == 0 ? 1ULL : token;
}

uint64_t RuntimeSession::NextChallengeNonce(const RuntimePeerAddress& peerAddress) const
{
  uint64_t nonce = MixU64(localSessionId ^ HashPeerAddress(peerAddress) ^ config.realmHash ^ 0xa4d1e57f9b8c1234ULL ^ (uint64_t)nextPacketSequence ^ (nowMs << 1));
  return nonce == 0 ? 1ULL : nonce;
}

bool RuntimeSession::AddPeerStrike(const RuntimePeerAddress& peerAddress, const std::string& reason, uint32_t suspicionDelta, uint32_t strikeDelta)
{
  const uint64_t quarantineDurationMs = std::max<uint64_t>(config.topologyBroadcastMs * 4ULL, 3000ULL);
  const ActiveNodeBucketResult result = knownNodes.AddPeerStrike(
      peerAddress,
      reason,
      suspicionDelta,
      strikeDelta,
      nowMs,
      quarantineDurationMs);
  if (result.code == ActiveNodeBucketCode::UnknownPeer) return false;

  connectedPeerAddresses.erase(
      std::remove_if(connectedPeerAddresses.begin(), connectedPeerAddresses.end(), [&](const RuntimePeerAddress& candidate)
                     { return RuntimePeerAddressEquals(candidate, peerAddress); }),
      connectedPeerAddresses.end());
  knownNodes.MarkConnected(peerAddress, false);
  DespawnRemotePlayer(peerAddress, nullptr);

  const char* action = result.code == ActiveNodeBucketCode::BannedPeer ? "banned" : result.code == ActiveNodeBucketCode::QuarantinedPeer ? "quarantined" : "flagged";
  TraceLog(LOG_WARNING, "runtime %s peer %s: %s", action, DescribeRuntimePeerAddress(peerAddress).c_str(), reason.c_str());
  return true;
}

bool RuntimeSession::IsPeerTrustedResponder(const RuntimePeerAddress& peerAddress) const
{
  const ActiveNodeState* node = knownNodes.FindByPeerAddress(peerAddress);
  if (node == nullptr) return false;
  return node->authenticated && !knownNodes.IsPeerIsolated(peerAddress, nowMs);
}

void RuntimeSession::RememberPeerHint(const TopologyNodeState& node, const RuntimePeerAddress& introducedBy)
{
  if (!IsValidPeerAddress(node.peerAddress) || !IsFinitePosition(node.position) || !IsFiniteInterestArea(node.interestArea)) return;
  if (RuntimePeerAddressEquals(node.peerAddress, config.bindAddress)) return;
  if (knownNodes.IsPeerBanned(node.peerAddress)) return;

  for (PeerHint& hint : peerHints)
  {
    if (!RuntimePeerAddressEquals(hint.node.peerAddress, node.peerAddress)) continue;
    if (hint.node.sessionId != 0 && hint.node.sessionId != node.sessionId) return;
    hint.node = node;
    hint.introducedBy = introducedBy;
    hint.lastHintMs = nowMs;
    return;
  }

  if (peerHints.size() >= config.maxKnownNodes)
  {
    std::sort(peerHints.begin(), peerHints.end(), [](const PeerHint& a, const PeerHint& b)
              { return a.lastHintMs < b.lastHintMs; });
    peerHints.erase(peerHints.begin());
  }

  peerHints.push_back({
      .node = node,
      .introducedBy = introducedBy,
      .lastHintMs = nowMs,
  });
}

void RuntimeSession::SendHandshakeToHintedPeers()
{
  HandshakePacketData handshake = BuildLocalHandshake();
  if (!SignHandshakePacket(&handshake)) return;

  for (const PeerHint& hint : peerHints)
  {
    if (IsHintExpired(nowMs, hint.lastHintMs)) continue;
    const ActiveNodeState* knownNode = knownNodes.FindByPeerAddress(hint.node.peerAddress);
    if (knownNode != nullptr && knownNode->sessionId == hint.node.sessionId && knownNode->authenticated) continue;
    SendPacketTo(hint.node.peerAddress, MakeHandshakePacket(handshake));
  }
}

void RuntimeSession::SendPacketTo(const RuntimePeerAddress& peerAddress, const Packet& packet)
{
  if (!config.enabled) return;
  if (peerAddress.host.empty() || peerAddress.port <= 0) return;
  if (knownNodes.IsPeerIsolated(peerAddress, nowMs)) return;
  const std::vector<uint8_t> bytes = SerializePacket(packet);
  const bool sent = client.SendPacket(peerAddress, bytes);
  if (!sent)
  {
    TraceLog(
        LOG_WARNING,
        "runtime send failed type=%s peer=%s bytes=%d",
        DescribePacketType(packet.type).c_str(),
        DescribeRuntimePeerAddress(peerAddress).c_str(),
        (int)bytes.size());
  }
}

void RuntimeSession::SendPacketToConnectedPeers(const Packet& packet)
{
  for (const RuntimePeerAddress& peerAddress : connectedPeerAddresses)
  {
    const ActiveNodeState* node = knownNodes.FindByPeerAddress(peerAddress);
    if (node == nullptr || !node->authenticated) continue;
    if (knownNodes.IsPeerIsolated(node->peerAddress, nowMs)) continue;
    SendPacketTo(node->peerAddress, packet);
  }
}
