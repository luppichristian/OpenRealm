#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#include "../TaskManager.h"
#include "../blockchain/Blockchain.h"
#include "../blockchain/BlockchainConfig.h"
#include "../blockchain/Wallet.h"
#include "../runtime/ActiveNodeBucket.h"
#include "../runtime/Packet.h"
#include "../runtime/RuntimeClient.h"
#include "../runtime/RuntimeHash.h"
#include "../runtime/RuntimeRealm.h"
#include "../world/World.h"
#include "../world/WorldConfig.h"
#include "../world/WorldEvent.h"

struct SimulatorConfig
{
  int frames = 0;
  float frameTime = 1.0f / 60.0f;
  int sleepMs = 16;
  bool runtimeEnabled = false;
  bool emitPlaceEvent = false;
  RuntimePeerAddress bindAddress = {"127.0.0.1", 46010};
  RuntimePeerAddress relayAddress = {"127.0.0.1", 46001};
  uint32_t localNodeId = 7001;
  int interestChunkX = 0;
  int interestChunkY = 0;
  uint32_t interestRadius = 1;
  uint8_t placeVoxelValue = 255;
  uint32_t receiveTimeoutMs = 1000;
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

struct SimulatorRuntimeState
{
  bool runtimeStarted = false;
  bool handshakeSent = false;
  bool discoveryReceived = false;
  bool discoveryParsed = false;
  bool discoveryDecoded = false;
  bool interestSent = false;
  bool worldEventSent = false;
  uint64_t realmHash = 0;
  size_t discoveredNodes = 0;
  int runtimePacketsReceived = 0;
  int runtimeWorldEventsReceived = 0;
  int runtimeWorldEventsApplied = 0;
  int runtimePlaceEventsApplied = 0;
};

static int ParseIntArgument(const char* value, int fallback)
{
  if (value == nullptr || value[0] == '\0') return fallback;

  char* end = nullptr;
  const long parsedValue = std::strtol(value, &end, 10);
  if (end == value || (end != nullptr && *end != '\0')) return fallback;
  return (int)parsedValue;
}

static float ParseFloatArgument(const char* value, float fallback)
{
  if (value == nullptr || value[0] == '\0') return fallback;

  char* end = nullptr;
  const float parsedValue = std::strtof(value, &end);
  if (end == value || (end != nullptr && *end != '\0')) return fallback;
  return parsedValue;
}

static SimulatorConfig ParseSimulatorConfig(int argc, char** argv)
{
  SimulatorConfig config = {};

  for (int i = 1; i < argc; ++i)
  {
    if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
    {
      config.frames = ParseIntArgument(argv[++i], config.frames);
      continue;
    }

    if (std::strcmp(argv[i], "--frame-time") == 0 && i + 1 < argc)
    {
      config.frameTime = ParseFloatArgument(argv[++i], config.frameTime);
      continue;
    }

    if (std::strcmp(argv[i], "--sleep-ms") == 0 && i + 1 < argc)
    {
      config.sleepMs = ParseIntArgument(argv[++i], config.sleepMs);
      continue;
    }

    if (std::strcmp(argv[i], "--no-sleep") == 0)
    {
      config.sleepMs = 0;
      continue;
    }

    if (std::strcmp(argv[i], "--runtime") == 0)
    {
      config.runtimeEnabled = true;
      continue;
    }

    if (std::strcmp(argv[i], "--bind-host") == 0 && i + 1 < argc)
    {
      config.runtimeEnabled = true;
      config.bindAddress.host = argv[++i];
      continue;
    }

    if (std::strcmp(argv[i], "--bind-port") == 0 && i + 1 < argc)
    {
      config.runtimeEnabled = true;
      config.bindAddress.port = ParseIntArgument(argv[++i], config.bindAddress.port);
      continue;
    }

    if (std::strcmp(argv[i], "--relay-host") == 0 && i + 1 < argc)
    {
      config.runtimeEnabled = true;
      config.relayAddress.host = argv[++i];
      continue;
    }

    if (std::strcmp(argv[i], "--relay-port") == 0 && i + 1 < argc)
    {
      config.runtimeEnabled = true;
      config.relayAddress.port = ParseIntArgument(argv[++i], config.relayAddress.port);
      continue;
    }

    if (std::strcmp(argv[i], "--node-id") == 0 && i + 1 < argc)
    {
      config.runtimeEnabled = true;
      config.localNodeId = (uint32_t)ParseIntArgument(argv[++i], (int)config.localNodeId);
      continue;
    }

    if (std::strcmp(argv[i], "--interest-x") == 0 && i + 1 < argc)
    {
      config.runtimeEnabled = true;
      config.interestChunkX = ParseIntArgument(argv[++i], config.interestChunkX);
      continue;
    }

    if (std::strcmp(argv[i], "--interest-y") == 0 && i + 1 < argc)
    {
      config.runtimeEnabled = true;
      config.interestChunkY = ParseIntArgument(argv[++i], config.interestChunkY);
      continue;
    }

    if (std::strcmp(argv[i], "--interest-radius") == 0 && i + 1 < argc)
    {
      config.runtimeEnabled = true;
      config.interestRadius = (uint32_t)ParseIntArgument(argv[++i], (int)config.interestRadius);
      continue;
    }

    if (std::strcmp(argv[i], "--emit-place") == 0)
    {
      config.runtimeEnabled = true;
      config.emitPlaceEvent = true;
      continue;
    }

    if (std::strcmp(argv[i], "--place-voxel") == 0 && i + 1 < argc)
    {
      config.runtimeEnabled = true;
      config.placeVoxelValue = (uint8_t)ParseIntArgument(argv[++i], (int)config.placeVoxelValue);
      continue;
    }

    if (std::strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc)
    {
      config.runtimeEnabled = true;
      config.receiveTimeoutMs = (uint32_t)ParseIntArgument(argv[++i], (int)config.receiveTimeoutMs);
      continue;
    }
  }

  if (config.frameTime <= 0.0f) config.frameTime = 1.0f / 60.0f;
  if (config.sleepMs < 0) config.sleepMs = 0;
  if (config.bindAddress.port <= 0) config.bindAddress.port = 46010;
  if (config.relayAddress.port <= 0) config.relayAddress.port = 46001;
  if (config.receiveTimeoutMs == 0) config.receiveTimeoutMs = 1000;
  return config;
}

static BlockchainConfig BuildSimulatorBlockchainConfig()
{
  BlockchainConfig blockchainConfig = {};
  blockchainConfig.rpcUrl = "http://127.0.0.1:8545";
  blockchainConfig.globalParamsAddress = "0x0000000000000000000000000000000000000000";
  blockchainConfig.playerRegistryAddress = "0x0000000000000000000000000000000000000000";
  blockchainConfig.chunkClaimsAddress = "0x0000000000000000000000000000000000000000";
  blockchainConfig.marketplaceAddress = "0x0000000000000000000000000000000000000000";
  return blockchainConfig;
}

static Wallet BuildSimulatorWallet()
{
  return Wallet("0x0000000000000000000000000000000000000001",
                "0x0000000000000000000000000000000000000001");
}

static RuntimeRealmState BuildSimulatorRealmState(Blockchain& blockchain, const BlockchainConfig& blockchainConfig)
{
  RuntimeRealmState realmState = {};
  const std::string chainId = blockchain.GetRpcClient().EthChainId();
  realmState.chainId = chainId.empty() ? "unavailable" : chainId;
  realmState.blockchainConfig = blockchainConfig;
  realmState.globalParams = blockchain.GetGlobalParams().GetState();
  return realmState;
}

static ReceivedPeerDiscoveryState ReceivePeerDiscovery(RuntimeClient& client, uint32_t timeoutMs)
{
  ReceivedPeerDiscoveryState state = {};
  std::vector<uint8_t> bytes = {};
  if (!client.ReceivePacket(&bytes, &state.peerAddress, timeoutMs)) return state;

  state.received = true;
  state.parsed = TryParsePacket(bytes, &state.packet);
  state.decoded = state.parsed && TryDecodePeerDiscoveryPacket(state.packet, &state.peerDiscovery);
  return state;
}

static bool TryBuildWorldEvent(const RuntimeWorldEventState& runtimeEvent, WorldEvent* worldEvent)
{
  if (worldEvent == nullptr) return false;
  if (runtimeEvent.type > (uint8_t)WorldEventType::Place) return false;

  *worldEvent = {
      .type = (WorldEventType)runtimeEvent.type,
      .playerId = runtimeEvent.playerId,
      .voxelValue = runtimeEvent.voxelValue,
      .voxelX = runtimeEvent.voxelX,
      .voxelY = runtimeEvent.voxelY,
      .voxelZ = runtimeEvent.voxelZ,
      .playerX = runtimeEvent.playerX,
      .playerY = runtimeEvent.playerY,
      .playerZ = runtimeEvent.playerZ,
      .playerYaw = runtimeEvent.playerYaw,
      .playerPitch = runtimeEvent.playerPitch,
      .moveX = runtimeEvent.moveX,
      .moveY = runtimeEvent.moveY,
      .lookDeltaX = runtimeEvent.lookDeltaX,
      .lookDeltaY = runtimeEvent.lookDeltaY,
  };
  return true;
}

static bool ApplyRuntimeWorldEvent(World& world, const WorldEventPacketData& runtimeWorldEvent, SimulatorRuntimeState* runtimeState)
{
  if (runtimeState == nullptr) return false;

  WorldEvent worldEvent = {};
  if (!TryBuildWorldEvent(runtimeWorldEvent.event, &worldEvent)) return false;

  if (worldEvent.type == WorldEventType::Place)
  {
    world.GetVoxelWorld().SetVoxel(worldEvent.voxelX, worldEvent.voxelY, worldEvent.voxelZ, worldEvent.voxelValue);
    runtimeState->runtimeWorldEventsApplied += 1;
    runtimeState->runtimePlaceEventsApplied += 1;
    return true;
  }

  world.SendEvent(worldEvent);
  runtimeState->runtimeWorldEventsApplied += 1;
  return true;
}

static bool StartRuntimeSession(
    const SimulatorConfig& config,
    RuntimeClient* runtimeClient,
    SimulatorRuntimeState* runtimeState
)
{
  if (runtimeClient == nullptr || runtimeState == nullptr) return false;

  runtimeState->runtimeStarted = runtimeClient->Start(config.bindAddress);
  if (!runtimeState->runtimeStarted) return false;

  BlockchainConfig blockchainConfig = BuildSimulatorBlockchainConfig();
  Wallet wallet = BuildSimulatorWallet();
  Blockchain blockchain(blockchainConfig, std::move(wallet));
  const RuntimeRealmState realmState = BuildSimulatorRealmState(blockchain, blockchainConfig);
  runtimeState->realmHash = ComputeRuntimeRealmHash(realmState);

  HandshakePacketData handshake = {};
  handshake.protocolVersion = 1;
  handshake.nodeId = config.localNodeId;
  handshake.realmHash = runtimeState->realmHash;
  runtimeState->handshakeSent = runtimeClient->SendPacket(
      config.relayAddress,
      SerializePacket(MakeHandshakePacket(handshake)));

  ReceivedPeerDiscoveryState peerDiscovery = ReceivePeerDiscovery(*runtimeClient, config.receiveTimeoutMs);
  runtimeState->discoveryReceived = peerDiscovery.received;
  runtimeState->discoveryParsed = peerDiscovery.parsed;
  runtimeState->discoveryDecoded = peerDiscovery.decoded;
  runtimeState->discoveredNodes = peerDiscovery.peerDiscovery.nodes.size();

  ChunkInterestPacketData chunkInterest = {};
  chunkInterest.nodeId = config.localNodeId;
  chunkInterest.chunkX = config.interestChunkX;
  chunkInterest.chunkY = config.interestChunkY;
  chunkInterest.radius = config.interestRadius;
  runtimeState->interestSent = runtimeClient->SendPacket(
      config.relayAddress,
      SerializePacket(MakeChunkInterestPacket(chunkInterest)));

  if (config.emitPlaceEvent)
  {
    WorldEventPacketData worldEvent = {};
    worldEvent.nodeId = config.localNodeId;
    worldEvent.event.type = (uint8_t)WorldEventType::Place;
    worldEvent.event.playerId = 1;
    worldEvent.event.voxelValue = config.placeVoxelValue;
    worldEvent.event.voxelX = config.interestChunkX * CHUNK_SIZE_XZ;
    worldEvent.event.voxelY = config.interestChunkY * CHUNK_SIZE_XZ;
    worldEvent.event.voxelZ = 1;
    runtimeState->worldEventSent = runtimeClient->SendPacket(
        config.relayAddress,
        SerializePacket(MakeWorldEventPacket(worldEvent)));
  }

  return runtimeState->handshakeSent && runtimeState->interestSent
      && (!config.emitPlaceEvent || runtimeState->worldEventSent);
}

static void PumpRuntimePackets(
    RuntimeClient& runtimeClient,
    const SimulatorConfig& config,
    World& world,
    SimulatorRuntimeState* runtimeState
)
{
  if (runtimeState == nullptr) return;

  for (;;)
  {
    std::vector<uint8_t> bytes = {};
    RuntimePeerAddress peerAddress = {};
    if (!runtimeClient.ReceivePacket(&bytes, &peerAddress, 0)) break;

    runtimeState->runtimePacketsReceived += 1;
    if (!RuntimePeerAddressEquals(peerAddress, config.relayAddress)) continue;

    Packet packet = {};
    if (!TryParsePacket(bytes, &packet)) continue;

    if (packet.type == PacketType::PeerDiscovery)
    {
      PeerDiscoveryPacketData peerDiscovery = {};
      runtimeState->discoveryReceived = true;
      runtimeState->discoveryParsed = true;
      runtimeState->discoveryDecoded = TryDecodePeerDiscoveryPacket(packet, &peerDiscovery);
      runtimeState->discoveredNodes = runtimeState->discoveryDecoded ? peerDiscovery.nodes.size() : runtimeState->discoveredNodes;
      continue;
    }

    if (packet.type != PacketType::WorldEvent) continue;

    WorldEventPacketData worldEvent = {};
    if (!TryDecodeWorldEventPacket(packet, &worldEvent)) continue;

    runtimeState->runtimeWorldEventsReceived += 1;
    ApplyRuntimeWorldEvent(world, worldEvent, runtimeState);
  }
}

int main(int argc, char** argv)
{
  const SimulatorConfig config = ParseSimulatorConfig(argc, argv);

  static TaskManager taskManager = {};
  if (!taskManager.Start(MESH_WORKER_COUNT))
  {
    TraceLog(LOG_WARNING, "Failed to start simulator task manager; background tasks will be disabled.");
  }

  static World world = {};
  world.Initialize();

  RuntimeClient runtimeClient = {};
  SimulatorRuntimeState runtimeState = {};
  bool runtimeOk = true;
  if (config.runtimeEnabled)
  {
    runtimeOk = StartRuntimeSession(config, &runtimeClient, &runtimeState);
  }

  int simulatedFrames = 0;
  while (config.frames <= 0 || simulatedFrames < config.frames)
  {
    if (config.runtimeEnabled && runtimeState.runtimeStarted)
    {
      PumpRuntimePackets(runtimeClient, config, world, &runtimeState);
    }

    world.Update(config.frameTime);
    simulatedFrames += 1;

    if (config.sleepMs > 0)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(config.sleepMs));
    }
  }

  if (config.runtimeEnabled && runtimeState.runtimeStarted)
  {
    PumpRuntimePackets(runtimeClient, config, world, &runtimeState);
  }

  const uint8_t remoteInterestVoxel = world.GetVoxelWorld().GetVoxel(
      config.interestChunkX * CHUNK_SIZE_XZ,
      config.interestChunkY * CHUNK_SIZE_XZ,
      1);

  int playerCount = 0;
  for (const PlayerState& player : world.GetPlayerSystem().GetPlayers())
  {
    if (player.active) playerCount += 1;
  }

  runtimeClient.Stop();
  world.Shutdown();
  taskManager.Stop();

  std::cout << "OpenRealm simulator node\n";
  std::cout << "- simulated frames: " << simulatedFrames << "\n";
  std::cout << "- frame time: " << config.frameTime << "\n";
  std::cout << "- sleep ms: " << config.sleepMs << "\n";
  std::cout << "- task manager running: " << (taskManager.IsRunning() ? "yes" : "no") << "\n";
  std::cout << "- player count before shutdown: " << playerCount << "\n";
  std::cout << "- runtime mode: " << (config.runtimeEnabled ? "enabled" : "disabled") << "\n";
  std::cout << "- runtime bind address: " << DescribeRuntimePeerAddress(config.bindAddress) << "\n";
  std::cout << "- runtime relay address: " << DescribeRuntimePeerAddress(config.relayAddress) << "\n";
  std::cout << "- runtime local node id: " << config.localNodeId << "\n";
  std::cout << "- runtime realm hash: " << FormatHash64(runtimeState.realmHash) << "\n";
  std::cout << "- runtime listener startup: " << (runtimeState.runtimeStarted ? "ok" : "failed") << "\n";
  std::cout << "- runtime handshake send: " << (runtimeState.handshakeSent ? "ok" : "failed") << "\n";
  std::cout << "- runtime discovery receive: " << (runtimeState.discoveryReceived ? "ok" : "failed") << "\n";
  std::cout << "- runtime discovery parse: " << (runtimeState.discoveryParsed ? "ok" : "failed") << "\n";
  std::cout << "- runtime discovery decode: " << (runtimeState.discoveryDecoded ? "ok" : "failed") << "\n";
  std::cout << "- runtime discovered nodes: " << runtimeState.discoveredNodes << "\n";
  std::cout << "- runtime chunk interest send: " << (runtimeState.interestSent ? "ok" : "failed") << "\n";
  std::cout << "- runtime place event send: " << (config.emitPlaceEvent ? (runtimeState.worldEventSent ? "ok" : "failed") : "skipped") << "\n";
  std::cout << "- runtime packets received: " << runtimeState.runtimePacketsReceived << "\n";
  std::cout << "- runtime world events received: " << runtimeState.runtimeWorldEventsReceived << "\n";
  std::cout << "- runtime world events applied: " << runtimeState.runtimeWorldEventsApplied << "\n";
  std::cout << "- runtime place events applied: " << runtimeState.runtimePlaceEventsApplied << "\n";
  std::cout << "- runtime interest voxel value: " << (int)remoteInterestVoxel << "\n";
  return runtimeOk ? 0 : 1;
}
