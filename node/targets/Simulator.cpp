#include "../TaskManager.h"
#include "../blockchain/RealmConfigFiles.h"
#include "../runtime/NodeConfigFiles.h"
#include "../runtime/ProtocolVersion.h"
#include "../runtime/RuntimeRealm.h"
#include "../runtime/RuntimeSession.h"
#include "../world/World.h"

#include <chrono>
#include <thread>

namespace
{
uint64_t BuildRealmHash(const RealmConfigFiles& realmFiles)
{
  RuntimeRealmState runtimeRealmState = {};
  runtimeRealmState.runtimeProtocolVersion = kRuntimeProtocolVersion;
  runtimeRealmState.chainId = realmFiles.directory;
  runtimeRealmState.blockchainConfig = realmFiles.blockchainConfig;
  return ComputeRuntimeRealmHash(runtimeRealmState);
}
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
  if (!bootConfig.realmDirectory.empty()) nodeFiles.selectedRealm = bootConfig.realmDirectory;

  RealmConfigFiles realmFiles = {};
  std::string realmError = {};
  if (!LoadRealmConfigFiles(nodeFiles.selectedRealm, &realmFiles, &realmError))
  {
    TraceLog(LOG_ERROR, "Failed to load realm config: %s", realmError.c_str());
    return 1;
  }

  RuntimePeerAddress jumpNode = {};
  if (!realmFiles.jumpNodes.empty() && nodeFiles.runtimeJumpNodeIndex >= 0
      && (size_t)nodeFiles.runtimeJumpNodeIndex < realmFiles.jumpNodes.size())
  {
    jumpNode = realmFiles.jumpNodes[(size_t)nodeFiles.runtimeJumpNodeIndex].peerAddress;
  }

  RuntimeSessionConfig runtimeConfig = {};
  runtimeConfig.role = RuntimeNodeRole::Simulator;
  runtimeConfig.bindAddress = nodeFiles.runtimeBindAddress;
  runtimeConfig.jumpNode = jumpNode;
  runtimeConfig.localNodeId = nodeFiles.runtimeNodeId;
  runtimeConfig.realmHash = BuildRealmHash(realmFiles);
  runtimeConfig.initialNodePosition = nodeFiles.runtimePosition;
  runtimeConfig.interestArea = nodeFiles.runtimeInterestArea;
  runtimeConfig.joinTargetPosition = nodeFiles.runtimeJoinTargetPosition;
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
    TraceLog(LOG_ERROR, "Failed to start simulator runtime session on %s:%d", runtimeConfig.bindAddress.host.c_str(), runtimeConfig.bindAddress.port);
    return 1;
  }

  static TaskManager taskManager = {};
  taskManager.Start(MESH_WORKER_COUNT);
  static World world = {};
  world.Initialize();

  const int maxFrames = nodeFiles.simulatorFrames;
  int frameIndex = 0;
  while (maxFrames <= 0 || frameIndex < maxFrames)
  {
    world.Update(nodeFiles.simulatorFrameTime);
    runtimeSession.Tick((uint32_t)(nodeFiles.simulatorFrameTime * 1000.0f + 0.5f), &world);
    if ((frameIndex % 60) == 0)
    {
      const RuntimeSessionStatus status = runtimeSession.GetStatus();
      TraceLog(
          LOG_INFO,
          "sim nodeId=%u known=%d connected=%d resolved=%d pos=(%.1f, %.1f, %.1f)",
          runtimeConfig.localNodeId,
          (int)status.knownNodes,
          (int)status.connectedNodes,
          status.joinResolved ? 1 : 0,
          status.localNodePosition.x,
          status.localNodePosition.y,
          status.localNodePosition.z);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(nodeFiles.simulatorSleepMs));
    ++frameIndex;
  }

  runtimeSession.Stop(&world);
  world.Shutdown();
  taskManager.Stop();
  return 0;
}
