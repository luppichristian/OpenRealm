#pragma once

#include "../blockchain/Wallet.h"
#include "RuntimeClient.h"

#include <cstdint>
#include <cstddef>
#include <string>

enum class RuntimeWorldNodeSelection : uint8_t
{
  Interest = 0,
  InterestOrBroadcast = 1,
  Broadcast = 2,
};

static constexpr uint32_t DEFAULT_RUNTIME_NODE_ID = 7001;
static constexpr uint32_t DEFAULT_RUNTIME_RECEIVE_TIMEOUT_MS = 1000;
static constexpr uint32_t DEFAULT_RUNTIME_INTEREST_RADIUS = 1;
static constexpr size_t DEFAULT_RUNTIME_MAX_NODE_CONNECTIONS = 32;
static constexpr size_t DEFAULT_RUNTIME_MAX_PEER_DISCOVERY_NODES = 32;
static constexpr size_t DEFAULT_RUNTIME_MAX_CHUNK_INTERESTS = 64;
static constexpr size_t DEFAULT_RUNTIME_MAX_WORLD_EVENT_RECIPIENTS = 32;

static constexpr uint32_t MAX_RUNTIME_RECEIVE_TIMEOUT_MS = 600000;
static constexpr uint32_t MAX_RUNTIME_INTEREST_RADIUS = 1024;
static constexpr size_t MAX_RUNTIME_NODE_CONNECTIONS = 256;
static constexpr size_t MAX_RUNTIME_PEER_DISCOVERY_NODES = 256;
static constexpr size_t MAX_RUNTIME_CHUNK_INTERESTS = 512;
static constexpr size_t MAX_RUNTIME_WORLD_EVENT_RECIPIENTS = 256;

std::string DescribeRuntimeWorldNodeSelection(RuntimeWorldNodeSelection selection);

struct NodeFilesConfig
{
  std::string configPath = "config.json";
  std::string selectedRealm = "realms/test";
  Wallet wallet = {};

  RuntimePeerAddress runtimeBindAddress = {"127.0.0.1", 46010};
  int runtimeJumpNodeIndex = 0;
  uint32_t runtimeNodeId = DEFAULT_RUNTIME_NODE_ID;
  int runtimeInterestChunkX = 0;
  int runtimeInterestChunkY = 0;
  uint32_t runtimeInterestRadius = DEFAULT_RUNTIME_INTEREST_RADIUS;
  uint32_t runtimeReceiveTimeoutMs = DEFAULT_RUNTIME_RECEIVE_TIMEOUT_MS;
  bool runtimeEnabled = false;
  RuntimeWorldNodeSelection runtimeWorldNodeSelection = RuntimeWorldNodeSelection::InterestOrBroadcast;
  size_t runtimeMaxNodeConnections = DEFAULT_RUNTIME_MAX_NODE_CONNECTIONS;
  size_t runtimeMaxPeerDiscoveryNodes = DEFAULT_RUNTIME_MAX_PEER_DISCOVERY_NODES;
  size_t runtimeMaxChunkInterests = DEFAULT_RUNTIME_MAX_CHUNK_INTERESTS;
  size_t runtimeMaxWorldEventRecipients = DEFAULT_RUNTIME_MAX_WORLD_EVENT_RECIPIENTS;

  int simulatorFrames = 0;
  float simulatorFrameTime = 1.0f / 60.0f;
  int simulatorSleepMs = 16;
  bool simulatorEmitPlaceEvent = false;
  uint8_t simulatorPlaceVoxelValue = 255;

  int relayTicks = 0;
};

struct NodeBootConfig
{
  std::string configPath = "config.json";
  std::string realmDirectory = {};
};

NodeBootConfig ParseNodeBootConfig(int argc, char** argv);
bool LoadNodeFilesConfig(const std::string& configPath, NodeFilesConfig* config, std::string* errorMessage);
