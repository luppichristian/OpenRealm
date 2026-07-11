#include "RealmTester.h"

#include "../node/runtime/ProtocolVersion.h"
#include "../node/runtime/RuntimeRealm.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

namespace
{
static constexpr uint64_t kHandshakeRetryMs = 500;
static constexpr uint64_t kChallengeRetryMs = 400;
static constexpr uint64_t kPlayerSnapshotPeriodMs = 200;
static constexpr uint64_t kReceiveTimeoutMs = 50;

uint64_t MixU64(uint64_t value)
{
  value ^= value >> 30;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31;
  return value;
}

uint64_t HashPeerAddress(const RuntimePeerAddress& peerAddress)
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

std::string DescribePeer(const RuntimePeerAddress& peerAddress)
{
  return peerAddress.host + ":" + std::to_string(peerAddress.port);
}

bool IsAvailableChain(const RuntimeRealmState& realmState)
{
  return !realmState.chainId.empty() && realmState.chainId != "unavailable";
}

RuntimeNodeRole DetermineLocalRole(const RuntimePeerAddress& bindAddress, const std::vector<RealmJumpNodeState>& jumpNodes)
{
  for (const RealmJumpNodeState& jumpNode : jumpNodes)
  {
    if (RuntimePeerAddressEquals(bindAddress, jumpNode.peerAddress))
    {
      return RuntimeNodeRole::Relay;
    }
  }
  return RuntimeNodeRole::Client;
}
}

RealmTester::RealmTester(const TesterOptions& options)
    : options(options),
      activeNodes(64)
{
}

bool RealmTester::Run(TesterSummary* summary)
{
  TesterSummary localSummary = {};
  auto finalize = [&](bool success, const std::string& errorMessage)
  {
    localSummary.success = success;
    localSummary.sawHandshake = sawHandshake;
    localSummary.sawChallengeRequest = sawChallengeRequest;
    localSummary.sawChallengeResponse = sawChallengeResponse;
    localSummary.sawJoinResponse = sawJoinResponse;
    localSummary.sawTopologySnapshot = sawTopologySnapshot;
    localSummary.sawPlayerSnapshot = sawPlayerSnapshot;
    localSummary.blockchainReady = blockchainReady;
    localSummary.maxTopologyNodes = maxTopologyNodes;
    localSummary.receivedPacketCounts = receivedPacketCounts;
    localSummary.sentPacketCounts = sentPacketCounts;
    localSummary.errorMessage = errorMessage;

    size_t authenticatedPeers = 0;
    for (const PeerProbeState& peerState : peerStates)
    {
      if (peerState.authenticated) authenticatedPeers += 1;
    }
    localSummary.authenticatedPeers = authenticatedPeers;
    localSummary.maxTopologyNodes = std::max(maxTopologyNodes, authenticatedPeers);

    Stop();
    if (summary != nullptr) *summary = localSummary;
    return success;
  };

  std::string errorMessage = {};
  if (!LoadInputs(&errorMessage)) return finalize(false, errorMessage);
  if (!Start(&errorMessage)) return finalize(false, errorMessage);

  const uint64_t startMs = NowMs();
  const uint64_t deadlineMs = startMs + (uint64_t)options.runSeconds * 1000;
  while (NowMs() < deadlineMs)
  {
    const uint64_t nowMs = NowMs();
    MaybeSendBootstrapHandshake(nowMs);
    MaybeSendHintedHandshakes(nowMs);
    MaybeSendJoinRequest(nowMs);
    MaybeSendPlayerSnapshot(nowMs);

    if (!PumpIncoming(nowMs, &errorMessage))
    {
      return finalize(false, errorMessage);
    }

    if (AllExpectationsSatisfied())
    {
      return finalize(true, {});
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  return finalize(false, BuildFailureMessage());
}

bool RealmTester::LoadInputs(std::string* errorMessage)
{
  std::string nodeError = {};
  if (!LoadNodeFilesConfig(options.configPath, &nodeFiles, &nodeError))
  {
    if (errorMessage != nullptr) *errorMessage = "failed to load node config: " + nodeError;
    return false;
  }

  std::string realmError = {};
  if (!LoadRealmConfigFiles(options.realmDirectory, &realmFiles, &realmError))
  {
    if (errorMessage != nullptr) *errorMessage = "failed to load realm config: " + realmError;
    return false;
  }

  if (options.jumpNodeIndex < 0 || (size_t)options.jumpNodeIndex >= realmFiles.jumpNodes.size())
  {
    if (errorMessage != nullptr) *errorMessage = "jump node index was out of range";
    return false;
  }

  jumpNode = realmFiles.jumpNodes[(size_t)options.jumpNodeIndex].peerAddress;
  bootstrapNodes.clear();
  for (const auto& entry : realmFiles.jumpNodes)
  {
    if (entry.peerAddress.host.empty() || entry.peerAddress.port <= 0) continue;
    bootstrapNodes.push_back(entry.peerAddress);
  }
  localRole = DetermineLocalRole(nodeFiles.runtimeBindAddress, realmFiles.jumpNodes);
  if (!ResolveRuntimeWalletAddress(nodeFiles.wallet, &localSignerAddress))
  {
    if (errorMessage != nullptr) *errorMessage = "failed to resolve runtime signer address from wallet";
    return false;
  }

  nodeFiles.runtimeBindAddress = options.bindAddress;
  if (nodeFiles.runtimeBindAddress.host.empty() || nodeFiles.runtimeBindAddress.port <= 0)
  {
    if (errorMessage != nullptr) *errorMessage = "invalid tester bind address";
    return false;
  }

  realmHash = BuildRealmHash();
  return true;
}

bool RealmTester::Start(std::string* errorMessage)
{
  if (!client.Start(nodeFiles.runtimeBindAddress))
  {
    if (errorMessage != nullptr) *errorMessage = "failed to start runtime client on " + DescribePeer(nodeFiles.runtimeBindAddress);
    return false;
  }

  localSessionId = NextSessionId();
  localChallengeNonce = NextChallengeNonce(nodeFiles.runtimeBindAddress);
  lastBootstrapHandshakeMs = 0;
  lastHintHandshakeMs = 0;
  lastPlayerSnapshotMs = 0;
  lastJoinRequestMs = 0;
  activeJoinRequestToken = 0;
  nextPacketSequence = 1;
  return true;
}

void RealmTester::Stop()
{
  client.Stop();
}

uint64_t RealmTester::BuildRealmHash()
{
  RuntimeRealmState runtimeRealmState = {};
  runtimeRealmState.runtimeProtocolVersion = kRuntimeProtocolVersion;
  runtimeRealmState.chainId = realmFiles.directory;
  runtimeRealmState.blockchainConfig = realmFiles.blockchainConfig;

  if (!realmFiles.blockchainConfig.rpcUrl.empty())
  {
    Blockchain blockchain(realmFiles.blockchainConfig, nodeFiles.wallet);
    const RuntimeRealmState enrichedState = BuildRuntimeRealmState(blockchain, realmFiles.blockchainConfig);
    blockchainReady = IsAvailableChain(enrichedState);
    if (blockchainReady)
    {
      runtimeRealmState = enrichedState;
    }
    else
    {
      runtimeRealmState.globalParams = enrichedState.globalParams;
    }
  }
  else
  {
    blockchainReady = false;
  }

  return ComputeRuntimeRealmHash(runtimeRealmState);
}

uint64_t RealmTester::NextSessionId() const
{
  static std::random_device device = {};
  static std::mt19937_64 generator(device());
  uint64_t value = 0;
  while (value == 0) value = generator();
  return value;
}

uint64_t RealmTester::NextChallengeNonce(const RuntimePeerAddress& peerAddress) const
{
  const uint64_t nowMs = NowMs();
  uint64_t nonce = MixU64(localSessionId ^ HashPeerAddress(peerAddress) ^ realmHash ^ 0xa4d1e57f9b8c1234ULL ^ (uint64_t)nextPacketSequence ^ (nowMs << 1));
  return nonce == 0 ? 1ULL : nonce;
}

uint64_t RealmTester::NextJoinRequestToken()
{
  const uint64_t nowMs = NowMs();
  uint64_t token = MixU64(localSessionId ^ (uint64_t)nextPacketSequence ^ (nowMs << 1) ^ 0x517cc1b727220a95ULL);
  return token == 0 ? 1ULL : token;
}

uint64_t RealmTester::NowMs() const
{
  return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

size_t RealmTester::PacketIndex(PacketType type) const
{
  return std::min<size_t>((size_t)type, receivedPacketCounts.size() - 1);
}

PacketPeerProof RealmTester::BuildLocalProof()
{
  PacketPeerProof proof = {};
  proof.sessionId = localSessionId;
  proof.sequence = nextPacketSequence++;
  return proof;
}

bool RealmTester::SignHandshakePacket(HandshakePacketData* handshake) const
{
  if (handshake == nullptr) return false;
  handshake->signerAddress = localSignerAddress;
  std::vector<uint8_t> messageBytes = {};
  if (!BuildHandshakeSigningMessage(*handshake, &messageBytes)) return false;
  std::string signerAddress = {};
  std::string signatureHex = {};
  if (!SignRuntimeAuthMessage(nodeFiles.wallet, messageBytes, &signerAddress, &signatureHex)) return false;
  if (NormalizeRuntimeAuthAddress(signerAddress) != NormalizeRuntimeAuthAddress(localSignerAddress)) return false;
  handshake->proof.signatureHex = signatureHex;
  return true;
}

bool RealmTester::SignChallengeRequestPacket(ChallengeRequestPacketData* challengeRequest) const
{
  if (challengeRequest == nullptr) return false;
  std::vector<uint8_t> messageBytes = {};
  if (!BuildChallengeRequestSigningMessage(*challengeRequest, &messageBytes)) return false;
  std::string signerAddress = {};
  std::string signatureHex = {};
  if (!SignRuntimeAuthMessage(nodeFiles.wallet, messageBytes, &signerAddress, &signatureHex)) return false;
  if (NormalizeRuntimeAuthAddress(signerAddress) != NormalizeRuntimeAuthAddress(localSignerAddress)) return false;
  challengeRequest->proof.signatureHex = signatureHex;
  return true;
}

bool RealmTester::SignChallengeResponsePacket(ChallengeResponsePacketData* challengeResponse) const
{
  if (challengeResponse == nullptr) return false;
  std::vector<uint8_t> messageBytes = {};
  if (!BuildChallengeResponseSigningMessage(*challengeResponse, &messageBytes)) return false;
  std::string signerAddress = {};
  std::string signatureHex = {};
  if (!SignRuntimeAuthMessage(nodeFiles.wallet, messageBytes, &signerAddress, &signatureHex)) return false;
  if (NormalizeRuntimeAuthAddress(signerAddress) != NormalizeRuntimeAuthAddress(localSignerAddress)) return false;
  challengeResponse->proof.signatureHex = signatureHex;
  return true;
}

bool RealmTester::SignJoinRequestPacket(JoinRequestPacketData* joinRequest) const
{
  if (joinRequest == nullptr) return false;
  std::vector<uint8_t> messageBytes = {};
  if (!BuildJoinRequestSigningMessage(*joinRequest, &messageBytes)) return false;
  std::string signerAddress = {};
  std::string signatureHex = {};
  if (!SignRuntimeAuthMessage(nodeFiles.wallet, messageBytes, &signerAddress, &signatureHex)) return false;
  if (NormalizeRuntimeAuthAddress(signerAddress) != NormalizeRuntimeAuthAddress(localSignerAddress)) return false;
  joinRequest->proof.signatureHex = signatureHex;
  return true;
}

bool RealmTester::SignPlayerSnapshotPacket(PlayerSnapshotPacketData* playerSnapshot) const
{
  if (playerSnapshot == nullptr) return false;
  std::vector<uint8_t> messageBytes = {};
  if (!BuildPlayerSnapshotSigningMessage(*playerSnapshot, &messageBytes)) return false;
  std::string signerAddress = {};
  std::string signatureHex = {};
  if (!SignRuntimeAuthMessage(nodeFiles.wallet, messageBytes, &signerAddress, &signatureHex)) return false;
  if (NormalizeRuntimeAuthAddress(signerAddress) != NormalizeRuntimeAuthAddress(localSignerAddress)) return false;
  playerSnapshot->proof.signatureHex = signatureHex;
  return true;
}

bool RealmTester::SendPacketTo(const RuntimePeerAddress& peerAddress, const Packet& packet, PacketType type)
{
  const std::vector<uint8_t> bytes = SerializePacket(packet);
  if (!client.SendPacket(peerAddress, bytes)) return false;
  const size_t index = PacketIndex(type);
  sentPacketCounts[index] += 1;
  return true;
}

void RealmTester::MaybeSendBootstrapHandshake(uint64_t nowMs)
{
  if (nowMs - lastBootstrapHandshakeMs < kHandshakeRetryMs) return;

  bool sent = false;
  for (const RuntimePeerAddress& bootstrapPeer : bootstrapNodes)
  {
    const ActiveNodeState* bootstrapState = activeNodes.FindByPeerAddress(bootstrapPeer);
    if (bootstrapState != nullptr && bootstrapState->authenticated) continue;

    HandshakePacketData handshake = {};
    handshake.proof = BuildLocalProof();
    handshake.protocolVersion = kRuntimeProtocolVersion;
    handshake.realmHash = realmHash;
    handshake.position = options.joinTargetPosition;
    handshake.interestArea = nodeFiles.runtimeInterestArea;
    handshake.nodeRole = (uint8_t)localRole;
    handshake.acceptsJoins = 0;
    handshake.challengeNonce = localChallengeNonce;
    if (!SignHandshakePacket(&handshake)) continue;
    if (SendPacketTo(bootstrapPeer, MakeHandshakePacket(handshake), PacketType::Handshake)) sent = true;
  }
  if (!sent) return;
  lastBootstrapHandshakeMs = nowMs;
}

void RealmTester::MaybeSendHintedHandshakes(uint64_t nowMs)
{
  if (nowMs - lastHintHandshakeMs < kHandshakeRetryMs) return;

  for (const PeerProbeState& peerState : peerStates)
  {
    if (!peerState.hinted || peerState.authenticated || IsBootstrapNode(peerState.peerAddress) || IsLocalPeerAddress(peerState.peerAddress)) continue;

    HandshakePacketData handshake = {};
    handshake.proof = BuildLocalProof();
    handshake.protocolVersion = kRuntimeProtocolVersion;
    handshake.realmHash = realmHash;
    handshake.position = options.joinTargetPosition;
    handshake.interestArea = nodeFiles.runtimeInterestArea;
    handshake.nodeRole = (uint8_t)localRole;
    handshake.acceptsJoins = 0;
    handshake.challengeNonce = localChallengeNonce;
    if (!SignHandshakePacket(&handshake)) continue;
    SendPacketTo(peerState.peerAddress, MakeHandshakePacket(handshake), PacketType::Handshake);
  }

  lastHintHandshakeMs = nowMs;
}

void RealmTester::MaybeSendJoinRequest(uint64_t nowMs)
{
  if (activeJoinRequestToken != 0 && nowMs - lastJoinRequestMs < kHandshakeRetryMs) return;
  if (nowMs - lastJoinRequestMs < kHandshakeRetryMs) return;
  if (activeJoinRequestToken != 0)
  {
    activeJoinRequestToken = 0;
  }

  const PeerProbeState* authenticatedPeer = nullptr;
  for (const PeerProbeState& peerState : peerStates)
  {
    if (!peerState.authenticated) continue;
    authenticatedPeer = &peerState;
    break;
  }
  if (authenticatedPeer == nullptr) return;

  JoinRequestPacketData joinRequest = {};
  joinRequest.proof = BuildLocalProof();
  joinRequest.targetPosition = options.joinTargetPosition;
  joinRequest.maxCandidates = (uint32_t)std::max<size_t>(options.expectMinTopologyNodes, 1);
  joinRequest.maxHops = nodeFiles.runtimeJoinMaxHops;
  joinRequest.requestToken = NextJoinRequestToken();
  if (!SignJoinRequestPacket(&joinRequest)) return;
  if (!SendPacketTo(authenticatedPeer->peerAddress, MakeJoinRequestPacket(joinRequest), PacketType::JoinRequest)) return;

  activeJoinRequestToken = joinRequest.requestToken;
  lastJoinRequestMs = nowMs;
}

void RealmTester::MaybeSendPlayerSnapshot(uint64_t nowMs)
{
  if (nowMs - lastPlayerSnapshotMs < kPlayerSnapshotPeriodMs) return;

  const PeerProbeState* authenticatedPeer = nullptr;
  for (const PeerProbeState& peerState : peerStates)
  {
    if (!peerState.authenticated) continue;
    authenticatedPeer = &peerState;
    break;
  }
  if (authenticatedPeer == nullptr) return;

  PlayerSnapshotPacketData playerSnapshot = {};
  playerSnapshot.proof = BuildLocalProof();
  playerSnapshot.tickMs = (uint32_t)std::min<uint64_t>(nowMs, UINT32_MAX);
  playerSnapshot.nodePosition = options.joinTargetPosition;
  playerSnapshot.playerPosition = options.joinTargetPosition;
  playerSnapshot.active = 1;
  if (!SignPlayerSnapshotPacket(&playerSnapshot)) return;
  if (!SendPacketTo(authenticatedPeer->peerAddress, MakePlayerSnapshotPacket(playerSnapshot), PacketType::PlayerSnapshot)) return;

  lastPlayerSnapshotMs = nowMs;
}

bool RealmTester::PumpIncoming(uint64_t nowMs, std::string* errorMessage)
{
  while (true)
  {
    std::vector<uint8_t> bytes = {};
    RuntimePeerAddress peerAddress = {};
    if (!client.ReceivePacket(&bytes, &peerAddress, (uint32_t)kReceiveTimeoutMs)) return true;
    if (!HandleIncoming(bytes, peerAddress, nowMs, errorMessage)) return false;
  }
}

bool RealmTester::HandleIncoming(const std::vector<uint8_t>& bytes, const RuntimePeerAddress& peerAddress, uint64_t nowMs, std::string* errorMessage)
{
  PacketValidationContext validationContext = {};
  validationContext.localPeerAddress = nodeFiles.runtimeBindAddress;
  validationContext.bootstrapPeerAddress = jumpNode;
  validationContext.expectedProtocolVersion = kRuntimeProtocolVersion;
  validationContext.expectedRealmHash = realmHash;
  validationContext.activeNodes = &activeNodes;
  validationContext.tick = nowMs;
  validationContext.maxKnownNodes = nodeFiles.runtimeMaxKnownNodes;
  validationContext.maxJoinCandidates = nodeFiles.runtimeJoinCandidateCount;
  validationContext.maxJoinHops = nodeFiles.runtimeJoinMaxHops;

  const PacketValidationResult validation = ValidateIncomingPacket(bytes, peerAddress, validationContext);
  if (!validation.accepted)
  {
    if (validation.code == PacketValidationCode::SelfPeerAddress)
    {
      return true;
    }
    if (errorMessage != nullptr)
    {
      *errorMessage = "runtime validation failed for peer " + DescribePeer(peerAddress) + ": " + DescribePacketValidationCode(validation.code);
    }
    return false;
  }

  const size_t index = PacketIndex(validation.packet.type);
  receivedPacketCounts[index] += 1;

  switch (validation.packet.type)
  {
    case PacketType::Handshake:         return HandleValidatedHandshake(validation, peerAddress, nowMs);
    case PacketType::ChallengeRequest:  return HandleValidatedChallengeRequest(validation, peerAddress, nowMs);
    case PacketType::ChallengeResponse: return HandleValidatedChallengeResponse(validation, peerAddress, errorMessage);
    case PacketType::JoinResponse:      return HandleValidatedJoinResponse(validation);
    case PacketType::TopologySnapshot:  return HandleValidatedTopologySnapshot(validation);
    case PacketType::PlayerSnapshot:    return HandleValidatedPlayerSnapshot(validation);
    default:                            return true;
  }
}

bool RealmTester::HandleValidatedHandshake(const PacketValidationResult& validation, const RuntimePeerAddress& peerAddress, uint64_t nowMs)
{
  sawHandshake = true;
  PeerProbeState* peerState = EnsurePeerState(peerAddress);
  if (peerState == nullptr) return false;

  if (!peerState->authenticated && peerState->pendingChallengeNonce == 0)
  {
    ChallengeRequestPacketData challengeRequest = {};
    challengeRequest.proof = BuildLocalProof();
    challengeRequest.challengeNonce = NextChallengeNonce(peerAddress);
    if (SignChallengeRequestPacket(&challengeRequest)
        && SendPacketTo(peerAddress, MakeChallengeRequestPacket(challengeRequest), PacketType::ChallengeRequest))
    {
      peerState->pendingChallengeNonce = challengeRequest.challengeNonce;
      peerState->lastChallengeRequestMs = nowMs;
      peerState->challengeRequestSent = true;
    }
  }

  if (!validation.handshake.signerAddress.empty())
  {
    std::cout << "- handshake accepted from " << DescribePeer(peerAddress)
              << " signer=" << validation.handshake.signerAddress << "\n";
  }
  return true;
}

bool RealmTester::HandleValidatedChallengeRequest(const PacketValidationResult& validation, const RuntimePeerAddress& peerAddress, uint64_t nowMs)
{
  sawChallengeRequest = true;
  ChallengeRequestPacketData challengeRequest = {};
  if (!TryDecodeChallengeRequestPacket(validation.packet, &challengeRequest)) return false;

  MarkPeerAuthenticated(peerAddress);

  ChallengeResponsePacketData challengeResponse = {};
  challengeResponse.proof = BuildLocalProof();
  challengeResponse.challengeNonce = challengeRequest.challengeNonce;
  if (!SignChallengeResponsePacket(&challengeResponse)) return false;
  if (!SendPacketTo(peerAddress, MakeChallengeResponsePacket(challengeResponse), PacketType::ChallengeResponse)) return false;

  PeerProbeState* peerState = EnsurePeerState(peerAddress);
  if (peerState != nullptr && !peerState->authenticated && peerState->pendingChallengeNonce == 0)
  {
    ChallengeRequestPacketData localChallenge = {};
    localChallenge.proof = BuildLocalProof();
    localChallenge.challengeNonce = NextChallengeNonce(peerAddress);
    if (SignChallengeRequestPacket(&localChallenge)
        && SendPacketTo(peerAddress, MakeChallengeRequestPacket(localChallenge), PacketType::ChallengeRequest))
    {
      peerState->pendingChallengeNonce = localChallenge.challengeNonce;
      peerState->lastChallengeRequestMs = nowMs;
      peerState->challengeRequestSent = true;
    }
  }

  return true;
}

bool RealmTester::HandleValidatedChallengeResponse(const PacketValidationResult& validation, const RuntimePeerAddress& peerAddress, std::string* errorMessage)
{
  sawChallengeResponse = true;
  ChallengeResponsePacketData challengeResponse = {};
  if (!TryDecodeChallengeResponsePacket(validation.packet, &challengeResponse)) return false;

  PeerProbeState* peerState = EnsurePeerState(peerAddress);
  if (peerState == nullptr) return false;
  if (peerState->pendingChallengeNonce == 0 || peerState->pendingChallengeNonce != challengeResponse.challengeNonce)
  {
    if (peerState->authenticated) return true;
    if (errorMessage != nullptr) *errorMessage = "challenge response nonce mismatch from " + DescribePeer(peerAddress);
    return false;
  }

  peerState->pendingChallengeNonce = 0;
  MarkPeerAuthenticated(peerAddress);
  std::cout << "- authenticated peer " << DescribePeer(peerAddress) << "\n";
  return true;
}

bool RealmTester::HandleValidatedJoinResponse(const PacketValidationResult& validation)
{
  sawJoinResponse = true;
  JoinResponsePacketData joinResponse = {};
  if (!TryDecodeJoinResponsePacket(validation.packet, &joinResponse, nodeFiles.runtimeMaxKnownNodes)) return false;
  if (activeJoinRequestToken == 0) return true;
  if (joinResponse.requestToken != activeJoinRequestToken) return true;
  activeJoinRequestToken = 0;

  maxTopologyNodes = std::max(maxTopologyNodes, joinResponse.candidates.size());
  for (const TopologyNodeState& candidate : joinResponse.candidates)
  {
    if (IsLocalPeerAddress(candidate.peerAddress)) continue;
    PeerProbeState* peerState = EnsurePeerState(candidate.peerAddress);
    if (peerState != nullptr) peerState->hinted = true;
  }

  std::cout << "- join response candidates=" << joinResponse.candidates.size() << "\n";
  return true;
}

bool RealmTester::HandleValidatedTopologySnapshot(const PacketValidationResult& validation)
{
  sawTopologySnapshot = true;
  TopologySnapshotPacketData topologySnapshot = {};
  if (!TryDecodeTopologySnapshotPacket(validation.packet, &topologySnapshot, nodeFiles.runtimeMaxKnownNodes)) return false;

  maxTopologyNodes = std::max(maxTopologyNodes, topologySnapshot.nodes.size());
  for (const TopologyNodeState& node : topologySnapshot.nodes)
  {
    if (IsLocalPeerAddress(node.peerAddress)) continue;
    PeerProbeState* peerState = EnsurePeerState(node.peerAddress);
    if (peerState != nullptr) peerState->hinted = true;
  }

  std::cout << "- topology snapshot nodes=" << topologySnapshot.nodes.size() << "\n";
  return true;
}

bool RealmTester::HandleValidatedPlayerSnapshot(const PacketValidationResult& validation)
{
  PlayerSnapshotPacketData playerSnapshot = {};
  if (!TryDecodePlayerSnapshotPacket(validation.packet, &playerSnapshot)) return false;
  if (playerSnapshot.active == 0) return true;
  sawPlayerSnapshot = true;
  return true;
}

bool RealmTester::IsBootstrapNode(const RuntimePeerAddress& peerAddress) const
{
  for (const RuntimePeerAddress& bootstrapPeer : bootstrapNodes)
  {
    if (RuntimePeerAddressEquals(bootstrapPeer, peerAddress)) return true;
  }
  return false;
}

bool RealmTester::IsLocalPeerAddress(const RuntimePeerAddress& peerAddress) const
{
  return RuntimePeerAddressEquals(peerAddress, nodeFiles.runtimeBindAddress);
}

RealmTester::PeerProbeState* RealmTester::FindPeerState(const RuntimePeerAddress& peerAddress)
{
  for (PeerProbeState& peerState : peerStates)
  {
    if (RuntimePeerAddressEquals(peerState.peerAddress, peerAddress)) return &peerState;
  }
  return nullptr;
}

const RealmTester::PeerProbeState* RealmTester::FindPeerState(const RuntimePeerAddress& peerAddress) const
{
  for (const PeerProbeState& peerState : peerStates)
  {
    if (RuntimePeerAddressEquals(peerState.peerAddress, peerAddress)) return &peerState;
  }
  return nullptr;
}

RealmTester::PeerProbeState* RealmTester::EnsurePeerState(const RuntimePeerAddress& peerAddress)
{
  if (IsLocalPeerAddress(peerAddress)) return nullptr;
  PeerProbeState* existing = FindPeerState(peerAddress);
  if (existing != nullptr) return existing;
  peerStates.push_back({.peerAddress = peerAddress});
  return &peerStates.back();
}

void RealmTester::MarkPeerAuthenticated(const RuntimePeerAddress& peerAddress)
{
  PeerProbeState* peerState = EnsurePeerState(peerAddress);
  if (peerState == nullptr) return;
  peerState->authenticated = true;

  ActiveNodeState* knownNode = activeNodes.FindMutableByPeerAddress(peerAddress);
  if (knownNode != nullptr) knownNode->authenticated = true;
}

bool RealmTester::AllExpectationsSatisfied() const
{
  size_t authenticatedPeers = 0;
  for (const PeerProbeState& peerState : peerStates)
  {
    if (peerState.authenticated) authenticatedPeers += 1;
  }

  const size_t topologyEvidence = std::max(maxTopologyNodes, authenticatedPeers);
  if (!sawHandshake || !sawChallengeRequest || !sawChallengeResponse || !sawJoinResponse) return false;
  if (authenticatedPeers == 0) return false;
  if (!sawTopologySnapshot && topologyEvidence < options.expectMinTopologyNodes) return false;
  if (topologyEvidence < options.expectMinTopologyNodes) return false;
  if (options.requirePlayerSnapshot && !sawPlayerSnapshot) return false;
  if (!blockchainReady && !realmFiles.blockchainConfig.rpcUrl.empty()) return false;
  return true;
}

std::string RealmTester::BuildFailureMessage() const
{
  std::vector<std::string> missing = {};
  size_t authenticatedPeers = 0;
  for (const PeerProbeState& peerState : peerStates)
  {
    if (peerState.authenticated) authenticatedPeers += 1;
  }
  const size_t topologyEvidence = std::max(maxTopologyNodes, authenticatedPeers);
  if (!sawHandshake) missing.push_back("handshake");
  if (!sawChallengeRequest) missing.push_back("challenge_request");
  if (!sawChallengeResponse) missing.push_back("challenge_response");
  if (!sawJoinResponse) missing.push_back("join_response");
  if (!sawTopologySnapshot && topologyEvidence < options.expectMinTopologyNodes) missing.push_back("topology_snapshot_or_multi_peer_auth");
  if (options.requirePlayerSnapshot && !sawPlayerSnapshot) missing.push_back("player_snapshot");
  if (topologyEvidence < options.expectMinTopologyNodes)
  {
    missing.push_back("topology_evidence>=" + std::to_string(options.expectMinTopologyNodes));
  }
  if (!blockchainReady && !realmFiles.blockchainConfig.rpcUrl.empty())
  {
    missing.push_back("reachable_test_blockchain");
  }

  std::ostringstream stream = {};
  stream << "tester expectations were not satisfied";
  if (!missing.empty())
  {
    stream << ": ";
    for (size_t i = 0; i < missing.size(); ++i)
    {
      if (i > 0) stream << ", ";
      stream << missing[i];
    }
  }
  return stream.str();
}
