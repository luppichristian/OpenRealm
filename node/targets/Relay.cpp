#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "../runtime/NodeConfigFiles.h"
#include "../cli/NodeCli.h"
#include "../blockchain/RealmConfigFiles.h"
#include "../blockchain/Blockchain.h"
#include "../blockchain/BlockchainConfig.h"
#include "../blockchain/Wallet.h"
#include "../runtime/ActiveNodeBucket.h"
#include "../runtime/ChunkInterestBucket.h"
#include "../runtime/Packet.h"
#include "../runtime/PacketValidator.h"
#include "../runtime/RuntimeClient.h"
#include "../runtime/RuntimeHash.h"
#include "../runtime/RuntimeRealm.h"
#include "../world/WorldEvent.h"

struct RelayConfig
{
  bool smoke = false;
  RuntimePeerAddress bindAddress = {"127.0.0.1", 46001};
  uint32_t localNodeId = 9001;
  uint32_t receiveTimeoutMs = 100;
  int ticks = 0;
  std::string configPath = "config.json";
  std::string realmDirectory = "realms/test";
  std::string realmName = {};
  size_t jumpNodeCount = 0;
  BlockchainConfig blockchainConfig = {};
  Wallet wallet = {};
};

struct RelayRuntimeStats
{
  int processedPackets = 0;
  int acceptedPackets = 0;
  int sentDiscoveryPackets = 0;
  int acceptedInterestPackets = 0;
  int forwardedWorldEvents = 0;
  int forwardedWorldEventRecipients = 0;
};

struct ReceivedPacketState
{
  bool received = false;
  RuntimePeerAddress peerAddress = {};
  PacketValidationResult validation = {};
};

struct ReceivedPeerDiscoveryState
{
  bool received = false;
  RuntimePeerAddress peerAddress = {};
  Packet packet = {};
  PeerDiscoveryPacketData peerDiscovery = {};
  bool parsed = false;
  bool decoded = false;
};

struct ReceivedWorldEventState
{
  bool received = false;
  RuntimePeerAddress peerAddress = {};
  Packet packet = {};
  WorldEventPacketData worldEvent = {};
  bool parsed = false;
  bool decoded = false;
};

static RelayConfig BuildRelayConfigFromFiles(const NodeFilesConfig& nodeFiles, const RealmConfigFiles& realmFiles)
{
  RelayConfig config = {};
  config.bindAddress = nodeFiles.relayBindAddress;
  config.localNodeId = nodeFiles.relayNodeId;
  config.receiveTimeoutMs = nodeFiles.relayReceiveTimeoutMs;
  config.ticks = nodeFiles.relayTicks;
  config.configPath = nodeFiles.configPath;
  config.realmDirectory = realmFiles.directory;
  config.realmName = realmFiles.realmName;
  config.jumpNodeCount = realmFiles.jumpNodes.size();
  config.blockchainConfig = realmFiles.blockchainConfig;
  config.wallet = nodeFiles.wallet;
  return config;
}

static void ApplyRelayArguments(int argc, char** argv, RelayConfig* config)
{
  if (config == nullptr) return;

  for (int i = 1; i < argc; ++i)
  {
    if ((std::strcmp(argv[i], "--config") == 0 || std::strcmp(argv[i], "--realm-dir") == 0) && i + 1 < argc)
    {
      ++i;
      continue;
    }

    if (std::strcmp(argv[i], "--smoke") == 0)
    {
      config->smoke = true;
      continue;
    }
  }

  if (config->bindAddress.port <= 0) config->bindAddress.port = 46001;
  if (config->receiveTimeoutMs == 0) config->receiveTimeoutMs = 100;
}

static ReceivedPacketState ReceiveAndValidate(
    RuntimeClient& listener,
    const PacketValidationContext& validationContext,
    uint32_t timeoutMs)
{
  ReceivedPacketState state = {};
  std::vector<uint8_t> bytes = {};
  if (!listener.ReceivePacket(&bytes, &state.peerAddress, timeoutMs)) return state;

  state.received = true;
  state.validation = ValidateIncomingPacket(bytes, state.peerAddress, validationContext);
  return state;
}

static ReceivedPeerDiscoveryState ReceivePeerDiscovery(RuntimeClient& listener, uint32_t timeoutMs)
{
  ReceivedPeerDiscoveryState state = {};
  std::vector<uint8_t> bytes = {};
  if (!listener.ReceivePacket(&bytes, &state.peerAddress, timeoutMs)) return state;

  state.received = true;
  state.parsed = TryParsePacket(bytes, &state.packet);
  state.decoded = state.parsed && TryDecodePeerDiscoveryPacket(state.packet, &state.peerDiscovery);
  return state;
}

static ReceivedWorldEventState ReceiveWorldEvent(RuntimeClient& listener, uint32_t timeoutMs)
{
  ReceivedWorldEventState state = {};
  std::vector<uint8_t> bytes = {};
  if (!listener.ReceivePacket(&bytes, &state.peerAddress, timeoutMs)) return state;

  state.received = true;
  state.parsed = TryParsePacket(bytes, &state.packet);
  state.decoded = state.parsed && TryDecodeWorldEventPacket(state.packet, &state.worldEvent);
  return state;
}

static const ActiveNodeState* FindKnownPeer(const ActiveNodeBucket& activeNodes, const RuntimePeerAddress& peerAddress)
{
  return activeNodes.FindByPeerAddress(peerAddress);
}

static bool TryDecodeKnownChunkInterest(
    const Packet& packet,
    const ActiveNodeState& activeNode,
    ChunkInterestPacketData* chunkInterest)
{
  if (!TryDecodeChunkInterestPacket(packet, chunkInterest)) return false;
  return chunkInterest->nodeId == activeNode.nodeId;
}

static bool TryDecodeKnownWorldEvent(
    const Packet& packet,
    const ActiveNodeState& activeNode,
    WorldEventPacketData* worldEvent)
{
  if (!TryDecodeWorldEventPacket(packet, worldEvent)) return false;
  return worldEvent->nodeId == activeNode.nodeId;
}

static void ProcessRelayPacket(
    RuntimeClient& relay,
    const ReceivedPacketState& state,
    uint64_t realmHash,
    ActiveNodeBucket* activeNodes,
    ChunkInterestBucket* chunkInterests,
    RelayRuntimeStats* stats)
{
  if (activeNodes == nullptr || chunkInterests == nullptr || stats == nullptr) return;
  if (!state.received) return;

  stats->processedPackets += 1;
  if (!state.validation.accepted) return;
  stats->acceptedPackets += 1;

  if (state.validation.packet.type == PacketType::Handshake)
  {
    PeerDiscoveryPacketData peerDiscovery = {};
    peerDiscovery.requestingNodeId = state.validation.handshake.nodeId;
    peerDiscovery.nodes = activeNodes->BuildPeerDiscoveryNodes(state.validation.handshake.nodeId, realmHash);
    const std::vector<uint8_t> discoveryBytes = SerializePacket(MakePeerDiscoveryPacket(peerDiscovery));
    if (relay.SendPacket(state.peerAddress, discoveryBytes))
    {
      stats->sentDiscoveryPackets += 1;
    }
    return;
  }

  const ActiveNodeState* activeNode = FindKnownPeer(*activeNodes, state.peerAddress);
  if (activeNode == nullptr) return;

  if (state.validation.packet.type == PacketType::ChunkInterest)
  {
    ChunkInterestPacketData chunkInterest = {};
    if (!TryDecodeKnownChunkInterest(state.validation.packet, *activeNode, &chunkInterest)) return;
    chunkInterests->RegisterInterest(chunkInterest);
    stats->acceptedInterestPackets += 1;
    return;
  }

  if (state.validation.packet.type != PacketType::WorldEvent) return;

  WorldEventPacketData worldEvent = {};
  if (!TryDecodeKnownWorldEvent(state.validation.packet, *activeNode, &worldEvent)) return;

  int chunkX = 0;
  int chunkY = 0;
  if (!chunkInterests->TryResolveWorldEventChunk(worldEvent, &chunkX, &chunkY)) return;

  const std::vector<RuntimePeerAddress> recipients = chunkInterests->BuildInterestedPeerAddresses(
      *activeNodes,
      worldEvent.nodeId,
      chunkX,
      chunkY);
  std::vector<RuntimePeerAddress> forwardingRecipients = recipients;
  if (forwardingRecipients.empty())
  {
    forwardingRecipients = activeNodes->BuildPeerAddresses(worldEvent.nodeId, realmHash);
  }
  if (forwardingRecipients.empty()) return;

  const std::vector<uint8_t> bytes = SerializePacket(state.validation.packet);
  bool forwardedAny = false;
  for (const RuntimePeerAddress& recipient : forwardingRecipients)
  {
    if (!relay.SendPacket(recipient, bytes)) continue;
    stats->forwardedWorldEventRecipients += 1;
    forwardedAny = true;
  }

  if (forwardedAny) stats->forwardedWorldEvents += 1;
}

static std::vector<NodeRuntimeViewLine> BuildRelayRuntimeViewLines(
    const RelayConfig& config,
    const RuntimeClient& relay,
    const RelayRuntimeStats& stats,
    const ActiveNodeBucket& activeNodes,
    const ChunkInterestBucket& chunkInterests,
    uint64_t realmHash,
    int idleTicks)
{
  return {
      {"Config path", config.configPath},
      {"Realm", config.realmDirectory},
      {"Bind", DescribeRuntimePeerAddress(relay.GetBindAddress())},
      {"Stack", relay.DescribeStack()},
      {"Node id", std::to_string(config.localNodeId)},
      {"Realm hash", FormatHash64(realmHash)},
      {"Ticks budget", std::to_string(config.ticks)},
      {"Idle ticks", std::to_string(idleTicks)},
      {"Processed packets", std::to_string(stats.processedPackets)},
      {"Accepted packets", std::to_string(stats.acceptedPackets)},
      {"Discovery packets", std::to_string(stats.sentDiscoveryPackets)},
      {"Interest packets", std::to_string(stats.acceptedInterestPackets)},
      {"World events", std::to_string(stats.forwardedWorldEvents)},
      {"World event recipients", std::to_string(stats.forwardedWorldEventRecipients)},
      {"Active nodes", std::to_string(activeNodes.GetCount())},
      {"Chunk interests", std::to_string(chunkInterests.GetCount())},
  };
}

static int RunRelaySmoke(const RelayConfig& config)
{
  RuntimePeerAddress relayBindAddress = config.bindAddress;
  RuntimePeerAddress acceptedPeerBindAddress = {"127.0.0.1", 46000};
  RuntimePeerAddress discoveredPeerBindAddress = {"127.0.0.1", 46004};
  RuntimePeerAddress duplicatePeerBindAddress = {"127.0.0.1", 46002};
  RuntimePeerAddress mismatchedRealmPeerBindAddress = {"127.0.0.1", 46003};

  RuntimeClient relayListener = {};
  RuntimeClient acceptedPeer = {};
  RuntimeClient discoveredPeer = {};
  RuntimeClient duplicatePeer = {};
  RuntimeClient mismatchedRealmPeer = {};

  const bool relayStarted = relayListener.Start(relayBindAddress);
  const bool acceptedPeerStarted = acceptedPeer.Start(acceptedPeerBindAddress);
  const bool discoveredPeerStarted = discoveredPeer.Start(discoveredPeerBindAddress);
  const bool duplicatePeerStarted = duplicatePeer.Start(duplicatePeerBindAddress);
  const bool mismatchedRealmPeerStarted = mismatchedRealmPeer.Start(mismatchedRealmPeerBindAddress);

  Blockchain blockchain(config.blockchainConfig, config.wallet);

  const RuntimeRealmState realmState = BuildRuntimeRealmState(blockchain, config.blockchainConfig);
  const uint64_t realmHash = ComputeRuntimeRealmHash(realmState);
  const std::string chainId = realmState.chainId;
  const GlobalParamsState globalParamsState = realmState.globalParams;
  ResolvedRuntimeAccount runtimeAccount = blockchain.GetPlayerRegistry().ResolveRuntimeAccount(
      blockchain.GetWallet().GetActiveSignerAddress());
  ChunkRuntimeState chunkRuntimeState = blockchain.GetChunkClaims().GetChunkRuntimeState(
      0,
      0,
      blockchain.GetWallet().GetActiveSignerAddress());
  SaleState saleState = blockchain.GetMarketplace().GetSaleStateForChunk(0, 0);

  ActiveNodeBucket activeNodes = {};
  ChunkInterestBucket chunkInterests = {};
  RelayRuntimeStats stats = {};
  PacketValidationContext validationContext = {};
  validationContext.localNodeId = config.localNodeId;
  validationContext.expectedRealmHash = realmHash;
  validationContext.activeNodes = &activeNodes;

  HandshakePacketData acceptedHandshake = {};
  acceptedHandshake.protocolVersion = 1;
  acceptedHandshake.nodeId = 42;
  acceptedHandshake.realmHash = realmHash;

  HandshakePacketData discoveredHandshake = {};
  discoveredHandshake.protocolVersion = 1;
  discoveredHandshake.nodeId = 43;
  discoveredHandshake.realmHash = realmHash;

  HandshakePacketData duplicateHandshake = acceptedHandshake;

  HandshakePacketData mismatchedRealmHandshake = {};
  mismatchedRealmHandshake.protocolVersion = 1;
  mismatchedRealmHandshake.nodeId = 84;
  mismatchedRealmHandshake.realmHash = realmHash + 1;

  const std::vector<uint8_t> acceptedBytes = SerializePacket(MakeHandshakePacket(acceptedHandshake));
  const std::vector<uint8_t> discoveredBytes = SerializePacket(MakeHandshakePacket(discoveredHandshake));
  const std::vector<uint8_t> duplicateBytes = SerializePacket(MakeHandshakePacket(duplicateHandshake));
  const std::vector<uint8_t> mismatchedRealmBytes = SerializePacket(MakeHandshakePacket(mismatchedRealmHandshake));

  const bool sentAcceptedPacket = acceptedPeerStarted && acceptedPeer.SendPacket(relayBindAddress, acceptedBytes);
  ReceivedPacketState acceptedPacket = ReceiveAndValidate(relayListener, validationContext, 1000);
  ProcessRelayPacket(relayListener, acceptedPacket, realmHash, &activeNodes, &chunkInterests, &stats);
  ReceivedPeerDiscoveryState initialAcceptedPeerDiscovery = ReceivePeerDiscovery(acceptedPeer, 1000);

  const bool sentDiscoveredPacket = discoveredPeerStarted && discoveredPeer.SendPacket(relayBindAddress, discoveredBytes);
  ReceivedPacketState discoveredPacket = ReceiveAndValidate(relayListener, validationContext, 1000);
  ProcessRelayPacket(relayListener, discoveredPacket, realmHash, &activeNodes, &chunkInterests, &stats);
  ReceivedPeerDiscoveryState discoveredPeerDiscovery = ReceivePeerDiscovery(discoveredPeer, 1000);

  const bool sentAcceptedRefreshPacket = acceptedPeer.SendPacket(relayBindAddress, acceptedBytes);
  ReceivedPacketState acceptedRefreshPacket = ReceiveAndValidate(relayListener, validationContext, 1000);
  ProcessRelayPacket(relayListener, acceptedRefreshPacket, realmHash, &activeNodes, &chunkInterests, &stats);
  ReceivedPeerDiscoveryState acceptedPeerDiscovery = ReceivePeerDiscovery(acceptedPeer, 1000);

  const bool sentDuplicatePacket = duplicatePeerStarted && duplicatePeer.SendPacket(relayBindAddress, duplicateBytes);
  ReceivedPacketState duplicatePacket = ReceiveAndValidate(relayListener, validationContext, 1000);
  ProcessRelayPacket(relayListener, duplicatePacket, realmHash, &activeNodes, &chunkInterests, &stats);

  const bool sentMismatchedRealmPacket = mismatchedRealmPeerStarted && mismatchedRealmPeer.SendPacket(relayBindAddress, mismatchedRealmBytes);
  ReceivedPacketState mismatchedRealmPacket = ReceiveAndValidate(relayListener, validationContext, 1000);
  ProcessRelayPacket(relayListener, mismatchedRealmPacket, realmHash, &activeNodes, &chunkInterests, &stats);

  ChunkInterestPacketData acceptedInterest = {};
  acceptedInterest.nodeId = acceptedHandshake.nodeId;
  acceptedInterest.chunkX = 0;
  acceptedInterest.chunkY = 0;
  acceptedInterest.radius = 1;

  ChunkInterestPacketData discoveredInterest = {};
  discoveredInterest.nodeId = discoveredHandshake.nodeId;
  discoveredInterest.chunkX = 0;
  discoveredInterest.chunkY = 0;
  discoveredInterest.radius = 1;

  const bool sentAcceptedInterest = acceptedPeer.SendPacket(
      relayBindAddress,
      SerializePacket(MakeChunkInterestPacket(acceptedInterest)));
  ReceivedPacketState acceptedInterestPacket = ReceiveAndValidate(relayListener, validationContext, 1000);
  ProcessRelayPacket(relayListener, acceptedInterestPacket, realmHash, &activeNodes, &chunkInterests, &stats);

  const bool sentDiscoveredInterest = discoveredPeer.SendPacket(
      relayBindAddress,
      SerializePacket(MakeChunkInterestPacket(discoveredInterest)));
  ReceivedPacketState discoveredInterestPacket = ReceiveAndValidate(relayListener, validationContext, 1000);
  ProcessRelayPacket(relayListener, discoveredInterestPacket, realmHash, &activeNodes, &chunkInterests, &stats);

  WorldEventPacketData discoveredWorldEvent = {};
  discoveredWorldEvent.nodeId = discoveredHandshake.nodeId;
  discoveredWorldEvent.event.type = (uint8_t)WorldEventType::Place;
  discoveredWorldEvent.event.playerId = 7;
  discoveredWorldEvent.event.voxelValue = 255;
  discoveredWorldEvent.event.voxelX = 0;
  discoveredWorldEvent.event.voxelY = 0;
  discoveredWorldEvent.event.voxelZ = 5;

  const bool sentWorldEvent = discoveredPeer.SendPacket(
      relayBindAddress,
      SerializePacket(MakeWorldEventPacket(discoveredWorldEvent)));
  ReceivedPacketState worldEventPacket = ReceiveAndValidate(relayListener, validationContext, 1000);
  ProcessRelayPacket(relayListener, worldEventPacket, realmHash, &activeNodes, &chunkInterests, &stats);
  ReceivedWorldEventState forwardedWorldEvent = ReceiveWorldEvent(acceptedPeer, 1000);
  ReceivedWorldEventState senderEchoWorldEvent = ReceiveWorldEvent(discoveredPeer, 100);

  const PeerDiscoveryNodeState* firstDiscoveredNode = acceptedPeerDiscovery.peerDiscovery.nodes.empty()
                                                          ? nullptr
                                                          : &acceptedPeerDiscovery.peerDiscovery.nodes[0];

  std::cout << "OpenRealm relay stack\n";
  std::cout << "- relay mode: smoke\n";
  std::cout << "- runtime config load: ok\n";
  std::cout << "- runtime config path: " << config.configPath << "\n";
  std::cout << "- runtime realm directory: " << config.realmDirectory << "\n";
  std::cout << "- runtime realm name: " << config.realmName << "\n";
  std::cout << "- runtime jump nodes loaded: " << config.jumpNodeCount << "\n";
  std::cout << "- runtime blockchain rpc url: " << config.blockchainConfig.rpcUrl << "\n";
  std::cout << "- runtime wallet account: " << config.wallet.GetAccountAddress() << "\n";
  std::cout << "- runtime wallet active signer: " << config.wallet.GetActiveSignerAddress() << "\n";
  std::cout << "- runtime networking: " << relayListener.DescribeStack() << "\n";
  std::cout << "- relay listener startup: " << (relayStarted ? "ok" : "failed") << "\n";
  std::cout << "- accepted peer startup: " << (acceptedPeerStarted ? "ok" : "failed") << "\n";
  std::cout << "- discovered peer startup: " << (discoveredPeerStarted ? "ok" : "failed") << "\n";
  std::cout << "- duplicate peer startup: " << (duplicatePeerStarted ? "ok" : "failed") << "\n";
  std::cout << "- mismatched realm peer startup: " << (mismatchedRealmPeerStarted ? "ok" : "failed") << "\n";
  std::cout << "- runtime accepted packet bytes: " << acceptedBytes.size() << "\n";
  std::cout << "- runtime accepted packet checksum: " << acceptedPacket.validation.packetChecksum << "\n";
  std::cout << "- runtime realm hash: " << FormatHash64(realmHash) << "\n";
  std::cout << "- runtime active nodes: " << activeNodes.GetCount() << "\n";
  std::cout << "- runtime chunk interests: " << chunkInterests.GetCount() << "\n";
  std::cout << "- accepted packet send: " << (sentAcceptedPacket ? "ok" : "failed") << "\n";
  std::cout << "- accepted packet receive: " << (acceptedPacket.received ? "ok" : "failed") << "\n";
  std::cout << "- accepted packet peer: " << DescribeRuntimePeerAddress(acceptedPacket.peerAddress) << "\n";
  std::cout << "- accepted packet validation: " << DescribePacketValidationCode(acceptedPacket.validation.code) << "\n";
  std::cout << "- discovered packet send: " << (sentDiscoveredPacket ? "ok" : "failed") << "\n";
  std::cout << "- discovered packet receive: " << (discoveredPacket.received ? "ok" : "failed") << "\n";
  std::cout << "- discovered packet validation: " << DescribePacketValidationCode(discoveredPacket.validation.code) << "\n";
  std::cout << "- peer discovery packet receive: " << (acceptedPeerDiscovery.received ? "ok" : "failed") << "\n";
  std::cout << "- peer discovery packet parse: " << (acceptedPeerDiscovery.parsed ? "ok" : "failed") << "\n";
  std::cout << "- peer discovery packet decode: " << (acceptedPeerDiscovery.decoded ? "ok" : "failed") << "\n";
  std::cout << "- peer discovery node count: " << acceptedPeerDiscovery.peerDiscovery.nodes.size() << "\n";
  std::cout << "- peer discovery first node: "
            << (firstDiscoveredNode == nullptr ? std::string("none")
                                               : DescribeRuntimePeerAddress(firstDiscoveredNode->peerAddress))
            << "\n";
  std::cout << "- duplicate packet send: " << (sentDuplicatePacket ? "ok" : "failed") << "\n";
  std::cout << "- duplicate packet receive: " << (duplicatePacket.received ? "ok" : "failed") << "\n";
  std::cout << "- duplicate packet validation: " << DescribePacketValidationCode(duplicatePacket.validation.code) << "\n";
  std::cout << "- realm mismatch packet send: " << (sentMismatchedRealmPacket ? "ok" : "failed") << "\n";
  std::cout << "- realm mismatch packet receive: " << (mismatchedRealmPacket.received ? "ok" : "failed") << "\n";
  std::cout << "- realm mismatch packet validation: " << DescribePacketValidationCode(mismatchedRealmPacket.validation.code) << "\n";
  std::cout << "- accepted interest packet send: " << (sentAcceptedInterest ? "ok" : "failed") << "\n";
  std::cout << "- discovered interest packet send: " << (sentDiscoveredInterest ? "ok" : "failed") << "\n";
  std::cout << "- world event packet send: " << (sentWorldEvent ? "ok" : "failed") << "\n";
  std::cout << "- world event forward receive: " << (forwardedWorldEvent.received ? "ok" : "failed") << "\n";
  std::cout << "- world event forward parse: " << (forwardedWorldEvent.parsed ? "ok" : "failed") << "\n";
  std::cout << "- world event forward decode: " << (forwardedWorldEvent.decoded ? "ok" : "failed") << "\n";
  std::cout << "- world event sender echo: " << (senderEchoWorldEvent.received ? "unexpected" : "none") << "\n";
  std::cout << "- relay sent discovery packets: " << stats.sentDiscoveryPackets << "\n";
  std::cout << "- relay accepted interest packets: " << stats.acceptedInterestPackets << "\n";
  std::cout << "- relay forwarded world events: " << stats.forwardedWorldEvents << "\n";
  std::cout << "- relay forwarded world event recipients: " << stats.forwardedWorldEventRecipients << "\n";
  std::cout << "- blockchain backend: " << blockchain.GetRpcClient().DescribeStack() << "\n";
  std::cout << "- blockchain facade: " << blockchain.DescribeStack() << "\n";
  std::cout << "- blockchain rpc url: " << blockchain.GetRpcClient().GetRpcUrl() << "\n";
  std::cout << "- wallet state: " << blockchain.GetWallet().DescribeWallet() << "\n";
  std::cout << "- wallet account: " << blockchain.GetWallet().GetAccountAddress() << "\n";
  std::cout << "- wallet active signer: " << blockchain.GetWallet().GetActiveSignerAddress() << "\n";
  std::cout << "- eth_chainId probe: " << (chainId.empty() ? "unavailable" : chainId) << "\n";
  std::cout << "- global params wrapper: " << blockchain.GetGlobalParams().DescribeContract() << " @ " << blockchain.GetGlobalParams().GetContractAddress() << "\n";
  std::cout << "- player registry wrapper: " << blockchain.GetPlayerRegistry().DescribeContract() << " @ " << blockchain.GetPlayerRegistry().GetContractAddress() << "\n";
  std::cout << "- chunk claims wrapper: " << blockchain.GetChunkClaims().DescribeContract() << " @ " << blockchain.GetChunkClaims().GetContractAddress() << "\n";
  std::cout << "- marketplace wrapper: " << blockchain.GetMarketplace().DescribeContract() << " @ " << blockchain.GetMarketplace().GetContractAddress() << "\n";
  std::cout << "- global params available: " << (globalParamsState.available ? "yes" : "no") << "\n";
  std::cout << "- runtime account resolved: " << (runtimeAccount.available ? "yes" : "no") << "\n";
  std::cout << "- chunk runtime state available: " << (chunkRuntimeState.available ? "yes" : "no") << "\n";
  std::cout << "- sale state available: " << (saleState.available ? "yes" : "no") << "\n";

  const bool runtimeValidationOk = relayStarted && acceptedPeerStarted && discoveredPeerStarted && duplicatePeerStarted && mismatchedRealmPeerStarted && sentAcceptedPacket && acceptedPacket.received && acceptedPacket.validation.accepted && initialAcceptedPeerDiscovery.received && initialAcceptedPeerDiscovery.parsed && initialAcceptedPeerDiscovery.decoded && initialAcceptedPeerDiscovery.peerDiscovery.nodes.empty() && sentDiscoveredPacket && discoveredPacket.received && discoveredPacket.validation.accepted && discoveredPeerDiscovery.received && discoveredPeerDiscovery.parsed && discoveredPeerDiscovery.decoded && discoveredPeerDiscovery.peerDiscovery.nodes.size() == 1 && sentAcceptedRefreshPacket && acceptedRefreshPacket.received && acceptedRefreshPacket.validation.accepted && acceptedPeerDiscovery.received && acceptedPeerDiscovery.parsed && acceptedPeerDiscovery.decoded && acceptedPeerDiscovery.peerDiscovery.nodes.size() == 1 && firstDiscoveredNode != nullptr && firstDiscoveredNode->nodeId == discoveredHandshake.nodeId && RuntimePeerAddressEquals(firstDiscoveredNode->peerAddress, discoveredPeerBindAddress) && sentDuplicatePacket && duplicatePacket.received && duplicatePacket.validation.code == PacketValidationCode::DuplicateNodeId && sentMismatchedRealmPacket && mismatchedRealmPacket.received && mismatchedRealmPacket.validation.code == PacketValidationCode::RealmMismatch && sentAcceptedInterest && sentDiscoveredInterest && sentWorldEvent && forwardedWorldEvent.received && forwardedWorldEvent.parsed && forwardedWorldEvent.decoded && forwardedWorldEvent.worldEvent.nodeId == discoveredHandshake.nodeId && forwardedWorldEvent.worldEvent.event.type == (uint8_t)WorldEventType::Place && forwardedWorldEvent.worldEvent.event.voxelValue == 255 && !senderEchoWorldEvent.received && activeNodes.GetCount() == 2 && chunkInterests.GetCount() == 2 && stats.sentDiscoveryPackets == 3 && stats.acceptedInterestPackets == 2 && stats.forwardedWorldEvents == 1 && stats.forwardedWorldEventRecipients == 1;

  return runtimeValidationOk ? 0 : 1;
}

static int RunRelayService(const RelayConfig& config, bool interactiveLaunch)
{
  RuntimeClient relay = {};
  if (!relay.Start(config.bindAddress))
  {
    std::cout << "OpenRealm relay stack\n";
    std::cout << "- relay mode: service\n";
    std::cout << "- runtime config load: ok\n";
    std::cout << "- runtime config path: " << config.configPath << "\n";
    std::cout << "- runtime realm directory: " << config.realmDirectory << "\n";
    std::cout << "- relay listener startup: failed\n";
    return 1;
  }

  Blockchain blockchain(config.blockchainConfig, config.wallet);
  const RuntimeRealmState realmState = BuildRuntimeRealmState(blockchain, config.blockchainConfig);
  const uint64_t realmHash = ComputeRuntimeRealmHash(realmState);

  ActiveNodeBucket activeNodes = {};
  ChunkInterestBucket chunkInterests = {};
  RelayRuntimeStats stats = {};
  PacketValidationContext validationContext = {};
  validationContext.localNodeId = config.localNodeId;
  validationContext.expectedRealmHash = realmHash;
  validationContext.activeNodes = &activeNodes;

  bool runtimeViewActive = false;
  if (interactiveLaunch)
  {
    NodeRuntimeViewOptions runtimeViewOptions = {};
    runtimeViewOptions.role = NodeCliRole::Relay;
    runtimeViewOptions.title = "OpenRealm Relay";
    runtimeViewOptions.subtitle = "Live runtime controls";
    runtimeViewActive = BeginNodeRuntimeView(runtimeViewOptions);
  }

  int idleTicks = 0;
  bool stopRequested = false;
  while ((config.ticks <= 0 || idleTicks < config.ticks) && !stopRequested)
  {
    ReceivedPacketState state = ReceiveAndValidate(relay, validationContext, config.receiveTimeoutMs);
    if (!state.received)
    {
      idleTicks += 1;
    }
    else
    {
      ProcessRelayPacket(relay, state, realmHash, &activeNodes, &chunkInterests, &stats);
    }

    if (runtimeViewActive)
    {
      stopRequested = PumpNodeRuntimeView(
          "Relay running. Press Q or Esc to stop cleanly.",
          BuildRelayRuntimeViewLines(config, relay, stats, activeNodes, chunkInterests, realmHash, idleTicks));
    }
  }

  if (runtimeViewActive) EndNodeRuntimeView();

  std::cout << "OpenRealm relay stack\n";
  std::cout << "- relay mode: service\n";
  std::cout << "- runtime config load: ok\n";
  std::cout << "- runtime config path: " << config.configPath << "\n";
  std::cout << "- runtime realm directory: " << config.realmDirectory << "\n";
  std::cout << "- runtime realm name: " << config.realmName << "\n";
  std::cout << "- runtime jump nodes loaded: " << config.jumpNodeCount << "\n";
  std::cout << "- runtime blockchain rpc url: " << config.blockchainConfig.rpcUrl << "\n";
  std::cout << "- runtime wallet account: " << config.wallet.GetAccountAddress() << "\n";
  std::cout << "- runtime wallet active signer: " << config.wallet.GetActiveSignerAddress() << "\n";
  std::cout << "- runtime networking: " << relay.DescribeStack() << "\n";
  std::cout << "- relay listener startup: ok\n";
  std::cout << "- relay bind address: " << DescribeRuntimePeerAddress(relay.GetBindAddress()) << "\n";
  std::cout << "- relay local node id: " << config.localNodeId << "\n";
  std::cout << "- relay realm hash: " << FormatHash64(realmHash) << "\n";
  std::cout << "- relay ticks budget: " << config.ticks << "\n";
  std::cout << "- relay idle ticks: " << idleTicks << "\n";
  std::cout << "- relay processed packets: " << stats.processedPackets << "\n";
  std::cout << "- relay accepted packets: " << stats.acceptedPackets << "\n";
  std::cout << "- relay sent discovery packets: " << stats.sentDiscoveryPackets << "\n";
  std::cout << "- relay accepted interest packets: " << stats.acceptedInterestPackets << "\n";
  std::cout << "- relay forwarded world events: " << stats.forwardedWorldEvents << "\n";
  std::cout << "- relay forwarded world event recipients: " << stats.forwardedWorldEventRecipients << "\n";
  std::cout << "- relay active nodes: " << activeNodes.GetCount() << "\n";
  std::cout << "- relay chunk interests: " << chunkInterests.GetCount() << "\n";
  std::cout << "- relay quit requested: " << (stopRequested ? "yes" : "no") << "\n";
  return 0;
}
int main(int argc, char** argv)
{
  const NodeBootConfig bootConfig = ParseNodeBootConfig(argc, argv);
  bool interactiveLaunch = false;
  if (ShouldRunNodeCli(argc, argv))
  {
    interactiveLaunch = true;
    NodeCliOptions cliOptions = {};
    cliOptions.role = NodeCliRole::Relay;
    cliOptions.configPath = bootConfig.configPath;
    if (!RunNodeCli(cliOptions)) return 0;
  }

  NodeFilesConfig nodeFiles = {};
  std::string loadError = {};
  if (!LoadNodeFilesConfig(bootConfig.configPath, &nodeFiles, &loadError))
  {
    std::cout << "OpenRealm relay stack\n";
    std::cout << "- runtime config load: failed\n";
    std::cout << "- runtime config path: " << bootConfig.configPath << "\n";
    std::cout << "- runtime config error: " << loadError << "\n";
    return 1;
  }

  const std::string realmDirectory = bootConfig.realmDirectory.empty() ? nodeFiles.selectedRealm : bootConfig.realmDirectory;
  RealmConfigFiles realmFiles = {};
  if (!LoadRealmConfigFiles(realmDirectory, &realmFiles, &loadError))
  {
    std::cout << "OpenRealm relay stack\n";
    std::cout << "- runtime config load: failed\n";
    std::cout << "- runtime config path: " << nodeFiles.configPath << "\n";
    std::cout << "- runtime realm directory: " << realmDirectory << "\n";
    std::cout << "- runtime config error: " << loadError << "\n";
    return 1;
  }

  RelayConfig config = BuildRelayConfigFromFiles(nodeFiles, realmFiles);
  ApplyRelayArguments(argc, argv, &config);

  if (config.smoke) return RunRelaySmoke(config);
  return RunRelayService(config, interactiveLaunch);
}
