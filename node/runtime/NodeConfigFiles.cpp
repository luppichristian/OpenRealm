#include "NodeConfigFiles.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

static bool LoadJsonFile(const std::string& path, nlohmann::json* json, std::string* errorMessage)
{
  if (json == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "json output was null";
    return false;
  }

  std::ifstream stream(path);
  if (!stream.is_open())
  {
    if (errorMessage != nullptr) *errorMessage = "failed to open " + path;
    return false;
  }

  *json = nlohmann::json::parse(stream, nullptr, false);
  if (json->is_discarded())
  {
    if (errorMessage != nullptr) *errorMessage = "failed to parse " + path;
    return false;
  }

  return true;
}

static std::string ReadStringValue(const nlohmann::json& json, const char* key, const std::string& fallback)
{
  if (!json.is_object() || key == nullptr || !json.contains(key) || !json[key].is_string()) return fallback;
  return json[key].get<std::string>();
}

static int ReadIntValue(const nlohmann::json& json, const char* key, int fallback)
{
  if (!json.is_object() || key == nullptr || !json.contains(key) || !json[key].is_number_integer()) return fallback;
  return json[key].get<int>();
}

static uint32_t ReadU32Value(const nlohmann::json& json, const char* key, uint32_t fallback)
{
  if (!json.is_object() || key == nullptr || !json.contains(key) || !json[key].is_number_unsigned())
  {
    if (!json.is_object() || key == nullptr || !json.contains(key) || !json[key].is_number_integer()) return fallback;
    const int value = json[key].get<int>();
    return value >= 0 ? (uint32_t)value : fallback;
  }
  return json[key].get<uint32_t>();
}

static size_t ReadSizeValue(const nlohmann::json& json, const char* key, size_t fallback)
{
  return (size_t)ReadU32Value(json, key, (uint32_t)fallback);
}

static float ReadFloatValue(const nlohmann::json& json, const char* key, float fallback)
{
  if (!json.is_object() || key == nullptr || !json.contains(key) || !json[key].is_number()) return fallback;
  return json[key].get<float>();
}

static bool ReadBoolValue(const nlohmann::json& json, const char* key, bool fallback)
{
  if (!json.is_object() || key == nullptr || !json.contains(key) || !json[key].is_boolean()) return fallback;
  return json[key].get<bool>();
}

static RuntimePeerAddress ReadPeerAddressValue(const nlohmann::json& json, const RuntimePeerAddress& fallback)
{
  RuntimePeerAddress peerAddress = fallback;
  if (!json.is_object()) return peerAddress;
  peerAddress.host = ReadStringValue(json, "host", peerAddress.host);
  peerAddress.port = ReadIntValue(json, "port", peerAddress.port);
  return peerAddress;
}

static size_t ClampSizeValue(size_t value, size_t minimumValue, size_t maximumValue)
{
  return std::max(minimumValue, std::min(value, maximumValue));
}

static uint32_t ClampU32Value(uint32_t value, uint32_t minimumValue, uint32_t maximumValue)
{
  return std::max(minimumValue, std::min(value, maximumValue));
}

static int ClampIntValue(int value, int minimumValue, int maximumValue)
{
  return std::max(minimumValue, std::min(value, maximumValue));
}

std::string DescribeRuntimeWorldNodeSelection(RuntimeWorldNodeSelection selection)
{
  switch (selection)
  {
    case RuntimeWorldNodeSelection::Interest: return "interest";
    case RuntimeWorldNodeSelection::InterestOrBroadcast: return "interest_or_broadcast";
    case RuntimeWorldNodeSelection::Broadcast: return "broadcast";
    default: return "interest_or_broadcast";
  }
}

static RuntimeWorldNodeSelection ParseRuntimeWorldNodeSelection(const std::string& value)
{
  if (value == "interest") return RuntimeWorldNodeSelection::Interest;
  if (value == "broadcast") return RuntimeWorldNodeSelection::Broadcast;
  return RuntimeWorldNodeSelection::InterestOrBroadcast;
}

NodeBootConfig ParseNodeBootConfig(int argc, char** argv)
{
  NodeBootConfig config = {};

  for (int i = 1; i < argc; ++i)
  {
    if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)
    {
      config.configPath = argv[++i];
      continue;
    }

    if (std::strcmp(argv[i], "--realm-dir") == 0 && i + 1 < argc)
    {
      config.realmDirectory = argv[++i];
      continue;
    }
  }

  return config;
}

bool LoadNodeFilesConfig(const std::string& configPath, NodeFilesConfig* config, std::string* errorMessage)
{
  if (config == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "node config output was null";
    return false;
  }

  nlohmann::json root = {};
  if (!LoadJsonFile(configPath, &root, errorMessage)) return false;

  config->configPath = configPath;
  config->selectedRealm = ReadStringValue(root, "realm", config->selectedRealm);

  const nlohmann::json walletJson = root.value("wallet", nlohmann::json::object());
  const std::string accountAddress = ReadStringValue(walletJson, "accountAddress", config->wallet.GetAccountAddress());
  config->wallet.Connect(accountAddress);

  const nlohmann::json runtimeJson = root.value("runtime", nlohmann::json::object());
  const nlohmann::json simulationJson = root.value("simulation", nlohmann::json::object());
  const nlohmann::json serviceJson = root.value("service", nlohmann::json::object());
  const nlohmann::json limitsJson = runtimeJson.value("limits", nlohmann::json::object());
  const nlohmann::json interestJson = runtimeJson.value("interest", nlohmann::json::object());

  config->runtimeBindAddress = ReadPeerAddressValue(runtimeJson.value("bindAddress", nlohmann::json::object()), config->runtimeBindAddress);
  config->runtimeJumpNodeIndex = ReadIntValue(runtimeJson, "jumpNodeIndex", config->runtimeJumpNodeIndex);
  config->runtimeNodeId = ReadU32Value(runtimeJson, "nodeId", config->runtimeNodeId);
  config->runtimeInterestChunkX = ReadIntValue(interestJson, "chunkX", config->runtimeInterestChunkX);
  config->runtimeInterestChunkY = ReadIntValue(interestJson, "chunkY", config->runtimeInterestChunkY);
  config->runtimeInterestRadius = ReadU32Value(interestJson, "radius", config->runtimeInterestRadius);
  config->runtimeReceiveTimeoutMs = ReadU32Value(runtimeJson, "receiveTimeoutMs", config->runtimeReceiveTimeoutMs);
  config->runtimeEnabled = ReadBoolValue(runtimeJson, "enabled", config->runtimeEnabled);
  config->runtimeWorldNodeSelection = ParseRuntimeWorldNodeSelection(
      ReadStringValue(runtimeJson, "worldNodeSelection", DescribeRuntimeWorldNodeSelection(config->runtimeWorldNodeSelection)));
  config->runtimeMaxNodeConnections = ReadSizeValue(limitsJson, "maxNodeConnections", config->runtimeMaxNodeConnections);
  config->runtimeMaxPeerDiscoveryNodes = ReadSizeValue(limitsJson, "maxPeerDiscoveryNodes", config->runtimeMaxPeerDiscoveryNodes);
  config->runtimeMaxChunkInterests = ReadSizeValue(limitsJson, "maxChunkInterests", config->runtimeMaxChunkInterests);
  config->runtimeMaxWorldEventRecipients = ReadSizeValue(limitsJson, "maxWorldEventRecipients", config->runtimeMaxWorldEventRecipients);

  config->simulatorFrames = ReadIntValue(simulationJson, "frames", config->simulatorFrames);
  config->simulatorFrameTime = ReadFloatValue(simulationJson, "frameTime", config->simulatorFrameTime);
  config->simulatorSleepMs = ReadIntValue(simulationJson, "sleepMs", config->simulatorSleepMs);
  config->simulatorEmitPlaceEvent = ReadBoolValue(simulationJson, "emitPlaceEvent", config->simulatorEmitPlaceEvent);
  config->simulatorPlaceVoxelValue = (uint8_t)ReadIntValue(simulationJson, "placeVoxelValue", (int)config->simulatorPlaceVoxelValue);

  config->relayTicks = ReadIntValue(serviceJson, "ticks", config->relayTicks);

  config->runtimeJumpNodeIndex = ClampIntValue(config->runtimeJumpNodeIndex, 0, 1024);
  config->runtimeInterestRadius = ClampU32Value(config->runtimeInterestRadius, 0, MAX_RUNTIME_INTEREST_RADIUS);
  config->runtimeReceiveTimeoutMs = ClampU32Value(config->runtimeReceiveTimeoutMs, 0, MAX_RUNTIME_RECEIVE_TIMEOUT_MS);
  config->runtimeMaxNodeConnections = ClampSizeValue(config->runtimeMaxNodeConnections, 1, MAX_RUNTIME_NODE_CONNECTIONS);
  config->runtimeMaxPeerDiscoveryNodes = ClampSizeValue(config->runtimeMaxPeerDiscoveryNodes, 1, MAX_RUNTIME_PEER_DISCOVERY_NODES);
  config->runtimeMaxChunkInterests = ClampSizeValue(config->runtimeMaxChunkInterests, 1, MAX_RUNTIME_CHUNK_INTERESTS);
  config->runtimeMaxWorldEventRecipients = ClampSizeValue(config->runtimeMaxWorldEventRecipients, 1, MAX_RUNTIME_WORLD_EVENT_RECIPIENTS);
  if (config->runtimeMaxPeerDiscoveryNodes > config->runtimeMaxNodeConnections)
  {
    config->runtimeMaxPeerDiscoveryNodes = config->runtimeMaxNodeConnections;
  }
  if (config->runtimeMaxWorldEventRecipients > config->runtimeMaxNodeConnections)
  {
    config->runtimeMaxWorldEventRecipients = config->runtimeMaxNodeConnections;
  }

  return true;
}
