#pragma once

#include "../blockchain/Wallet.h"
#include "RuntimeClient.h"
#include "RuntimeWorldPosition.h"

#include <cstddef>
#include <cstdint>
#include <string>

static constexpr uint32_t DEFAULT_RUNTIME_NODE_ID = 7001;
static constexpr uint32_t DEFAULT_RUNTIME_RECEIVE_TIMEOUT_MS = 50;
static constexpr uint32_t DEFAULT_RUNTIME_JOIN_CANDIDATE_COUNT = 6;
static constexpr uint32_t DEFAULT_RUNTIME_JOIN_MAX_HOPS = 8;
static constexpr size_t DEFAULT_RUNTIME_MAX_NODE_CONNECTIONS = 8;
static constexpr size_t DEFAULT_RUNTIME_MAX_KNOWN_NODES = 64;
static constexpr uint32_t DEFAULT_RUNTIME_NEIGHBOR_REFRESH_MS = 250;
static constexpr uint32_t DEFAULT_RUNTIME_TOPOLOGY_BROADCAST_MS = 500;
static constexpr uint32_t DEFAULT_RUNTIME_PLAYER_BROADCAST_MS = 50;

static constexpr uint32_t MAX_RUNTIME_RECEIVE_TIMEOUT_MS = 600000;
static constexpr uint32_t MAX_RUNTIME_JOIN_CANDIDATE_COUNT = 64;
static constexpr uint32_t MAX_RUNTIME_JOIN_MAX_HOPS = 64;
static constexpr size_t MAX_RUNTIME_NODE_CONNECTIONS = 256;
static constexpr size_t MAX_RUNTIME_KNOWN_NODES = 1024;
static constexpr uint32_t MAX_RUNTIME_PERIOD_MS = 600000;

struct NodeFilesConfig
{
  std::string configPath = "config.json";
  std::string selectedRealm = "realms/test";
  Wallet wallet = {};

  RuntimePeerAddress runtimeBindAddress = {"127.0.0.1", 46010};
  int runtimeJumpNodeIndex = 0;
  uint32_t runtimeNodeId = DEFAULT_RUNTIME_NODE_ID;
  bool runtimeEnabled = false;
  bool runtimeAcceptsJoins = true;
  RuntimeWorldPosition runtimePosition = {};
  RuntimeInterestArea runtimeInterestArea = {};
  RuntimeWorldPosition runtimeJoinTargetPosition = {};
  uint32_t runtimeReceiveTimeoutMs = DEFAULT_RUNTIME_RECEIVE_TIMEOUT_MS;
  size_t runtimeMaxNodeConnections = DEFAULT_RUNTIME_MAX_NODE_CONNECTIONS;
  size_t runtimeMaxKnownNodes = DEFAULT_RUNTIME_MAX_KNOWN_NODES;
  uint32_t runtimeJoinCandidateCount = DEFAULT_RUNTIME_JOIN_CANDIDATE_COUNT;
  uint32_t runtimeJoinMaxHops = DEFAULT_RUNTIME_JOIN_MAX_HOPS;
  uint32_t runtimeNeighborRefreshMs = DEFAULT_RUNTIME_NEIGHBOR_REFRESH_MS;
  uint32_t runtimeTopologyBroadcastMs = DEFAULT_RUNTIME_TOPOLOGY_BROADCAST_MS;
  uint32_t runtimePlayerBroadcastMs = DEFAULT_RUNTIME_PLAYER_BROADCAST_MS;

  int simulatorFrames = 0;
  float simulatorFrameTime = 1.0f / 60.0f;
  int simulatorSleepMs = 16;
  int relayTicks = 0;
};

struct NodeBootConfig
{
  std::string configPath = "config.json";
  std::string realmDirectory = {};
};

NodeBootConfig ParseNodeBootConfig(int argc, char** argv);
bool LoadNodeFilesConfig(const std::string& configPath, NodeFilesConfig* config, std::string* errorMessage);
