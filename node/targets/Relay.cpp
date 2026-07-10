#include "../blockchain/RealmConfigFiles.h"
#include "../runtime/NodeConfigFiles.h"
#include "../runtime/ProtocolVersion.h"
#include "../runtime/RuntimeRealm.h"
#include "../runtime/RuntimeSession.h"

#include <chrono>
#include <thread>

static uint64_t BuildRealmHash(const RealmConfigFiles& realmFiles, const Wallet& wallet)
{
  RuntimeRealmState runtimeRealmState = {};
  runtimeRealmState.runtimeProtocolVersion = kRuntimeProtocolVersion;
  runtimeRealmState.chainId = realmFiles.directory;
  runtimeRealmState.blockchainConfig = realmFiles.blockchainConfig;

  if (!realmFiles.blockchainConfig.rpcUrl.empty())
  {
    Blockchain blockchain(realmFiles.blockchainConfig, wallet);
    const RuntimeRealmState enrichedState = BuildRuntimeRealmState(blockchain, realmFiles.blockchainConfig);
    if (!enrichedState.chainId.empty() && enrichedState.chainId != "unavailable")
    {
      runtimeRealmState = enrichedState;
    }
    else
    {
      runtimeRealmState.globalParams = enrichedState.globalParams;
    }
  }

  return ComputeRuntimeRealmHash(runtimeRealmState);
}

int main(int argc, char** argv)
{
  const NodeBootConfig bootConfig = ParseNodeBootConfig(argc, argv);

  NodeFilesConfig nodeFiles = {};
  std::string nodeError = {};
  if (!LoadNodeFilesConfig(bootConfig.configPath, &nodeFiles, &nodeError))
  {
    TraceLog(LOG_ERROR, "Failed to load node config: %s", nodeError.c_str());
    return 1;
  }

  RealmConfigFiles realmFiles = {};
  std::string realmError = {};
  if (!LoadRealmConfigFiles(bootConfig.realmDirectory, &realmFiles, &realmError))
  {
    TraceLog(LOG_ERROR, "Failed to load realm config: %s", realmError.c_str());
    return 1;
  }

  RuntimePeerAddress jumpNode = {};
  if (!realmFiles.jumpNodes.empty() && bootConfig.jumpNodeIndex >= 0 && (size_t)bootConfig.jumpNodeIndex < realmFiles.jumpNodes.size())
  {
    jumpNode = realmFiles.jumpNodes[(size_t)bootConfig.jumpNodeIndex].peerAddress;
  }

  RuntimeSessionConfig runtimeConfig = {};
  runtimeConfig.role = RuntimeNodeRole::Relay;
  runtimeConfig.bindAddress = nodeFiles.runtimeBindAddress;
  runtimeConfig.jumpNode = jumpNode;
  runtimeConfig.wallet = nodeFiles.wallet;
  runtimeConfig.realmHash = BuildRealmHash(realmFiles, nodeFiles.wallet);
  runtimeConfig.initialNodePosition = nodeFiles.runtimePosition;
  runtimeConfig.interestArea = nodeFiles.runtimeInterestArea;
  runtimeConfig.joinTargetPosition = nodeFiles.runtimePosition;

  runtimeConfig.receiveTimeoutMs = nodeFiles.runtimeReceiveTimeoutMs;
  runtimeConfig.maxNodeConnections = nodeFiles.runtimeMaxNodeConnections;
  runtimeConfig.maxKnownNodes = nodeFiles.runtimeMaxKnownNodes;
  runtimeConfig.maxJoinCandidates = nodeFiles.runtimeJoinCandidateCount;

  runtimeConfig.maxJoinHops = nodeFiles.runtimeJoinMaxHops;
  runtimeConfig.neighborRefreshMs = nodeFiles.runtimeNeighborRefreshMs;
  runtimeConfig.topologyBroadcastMs = nodeFiles.runtimeTopologyBroadcastMs;
  runtimeConfig.playerBroadcastMs = nodeFiles.runtimePlayerBroadcastMs;
  runtimeConfig.enabled = nodeFiles.runtimeEnabled;
  runtimeConfig.acceptsJoins = nodeFiles.runtimeAcceptsJoins;

  RuntimeSession runtimeSession = {};
  if (!runtimeSession.Start(runtimeConfig))
  {
    TraceLog(LOG_ERROR, "Failed to start relay runtime session on %s:%d", runtimeConfig.bindAddress.host.c_str(), runtimeConfig.bindAddress.port);
    return 1;
  }

  const int maxTicks = nodeFiles.relayTicks;
  int tickCount = 0;
  while (maxTicks <= 0 || tickCount < maxTicks)
  {
    runtimeSession.Tick(16, nullptr);
    if ((tickCount % 60) == 0)
    {
      const RuntimeSessionStatus status = runtimeSession.GetStatus();
      TraceLog(
          LOG_INFO,
          "relay bind=%s:%d known=%d connected=%d pos=(%.1f, %.1f, %.1f)",
          runtimeConfig.bindAddress.host.c_str(),
          runtimeConfig.bindAddress.port,
          (int)status.knownNodes,
          (int)status.connectedNodes,
          status.localNodePosition.x,
          status.localNodePosition.y,
          status.localNodePosition.z);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    ++tickCount;
  }

  runtimeSession.Stop();
  return 0;
}
