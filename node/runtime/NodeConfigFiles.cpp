#include "NodeConfigFiles.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>

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
  if (!json.is_object() || key == nullptr || !json.contains(key) || !json[key].is_number()) return fallback;
  const int64_t value = json[key].get<int64_t>();
  return value >= 0 ? (uint32_t)value : fallback;
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

static RuntimeWorldPosition ReadWorldPositionValue(const nlohmann::json& json, const RuntimeWorldPosition& fallback)
{
  RuntimeWorldPosition position = fallback;
  if (!json.is_object()) return position;
  position.x = ReadFloatValue(json, "x", position.x);
  position.y = ReadFloatValue(json, "y", position.y);
  position.z = ReadFloatValue(json, "z", position.z);
  return position;
}

static RuntimeInterestArea ReadInterestAreaValue(const nlohmann::json& json, const RuntimeInterestArea& fallback)
{
  RuntimeInterestArea area = fallback;
  if (!json.is_object()) return area;
  area.radiusX = ReadFloatValue(json, "radiusX", area.radiusX);
  area.radiusY = ReadFloatValue(json, "radiusY", area.radiusY);
  area.radiusZ = ReadFloatValue(json, "radiusZ", area.radiusZ);
  return area;
}

static const nlohmann::json& ReadObjectOrEmpty(const nlohmann::json& json, const char* key)
{
  static const nlohmann::json empty = nlohmann::json::object();
  if (!json.is_object() || key == nullptr || !json.contains(key) || !json[key].is_object()) return empty;
  return json[key];
}

static size_t ClampSizeValue(size_t value, size_t minimumValue, size_t maximumValue)
{
  return std::max(minimumValue, std::min(value, maximumValue));
}

static uint32_t ClampU32Value(uint32_t value, uint32_t minimumValue, uint32_t maximumValue)
{
  return std::max(minimumValue, std::min(value, maximumValue));
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
    if (std::strcmp(argv[i], "--jump-node-index") == 0 && i + 1 < argc)
    {
      config.jumpNodeIndex = std::max(0, std::atoi(argv[++i]));
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

  const nlohmann::json& walletJson = ReadObjectOrEmpty(root, "wallet");
  config->wallet.Connect(ReadStringValue(root, "accountAddress", ReadStringValue(walletJson, "accountAddress", config->wallet.GetAccountAddress())));

  const nlohmann::json& runtimeJson = ReadObjectOrEmpty(root, "runtime");
  const nlohmann::json& joinJson = ReadObjectOrEmpty(root, "join");
  const nlohmann::json& legacyJoinJson = ReadObjectOrEmpty(runtimeJson, "join");
  const nlohmann::json& limitsJson = ReadObjectOrEmpty(root, "limits");
  const nlohmann::json& legacyLimitsJson = ReadObjectOrEmpty(runtimeJson, "limits");
  const nlohmann::json& periodsJson = ReadObjectOrEmpty(root, "periodsMs");
  const nlohmann::json& legacyPeriodsJson = ReadObjectOrEmpty(runtimeJson, "periodsMs");
  const nlohmann::json& simulationJson = ReadObjectOrEmpty(root, "simulation");
  const nlohmann::json& serviceJson = ReadObjectOrEmpty(root, "service");

  config->runtimeBindAddress = ReadPeerAddressValue(ReadObjectOrEmpty(root, "bindAddress"), ReadPeerAddressValue(ReadObjectOrEmpty(runtimeJson, "bindAddress"), config->runtimeBindAddress));
  config->runtimeEnabled = ReadBoolValue(root, "enabled", ReadBoolValue(runtimeJson, "enabled", config->runtimeEnabled));
  config->runtimeAcceptsJoins = ReadBoolValue(root, "acceptsJoins", ReadBoolValue(runtimeJson, "acceptsJoins", config->runtimeAcceptsJoins));
  config->runtimePosition = ReadWorldPositionValue(ReadObjectOrEmpty(root, "position"), ReadWorldPositionValue(ReadObjectOrEmpty(runtimeJson, "position"), config->runtimePosition));
  config->runtimeInterestArea = ReadInterestAreaValue(ReadObjectOrEmpty(root, "areaOfInterest"), ReadInterestAreaValue(ReadObjectOrEmpty(runtimeJson, "areaOfInterest"), config->runtimeInterestArea));
  config->runtimeReceiveTimeoutMs = ReadU32Value(root, "receiveTimeoutMs", ReadU32Value(runtimeJson, "receiveTimeoutMs", config->runtimeReceiveTimeoutMs));
  config->runtimeMaxNodeConnections = ReadSizeValue(limitsJson, "maxNodeConnections", ReadSizeValue(legacyLimitsJson, "maxNodeConnections", config->runtimeMaxNodeConnections));
  config->runtimeMaxKnownNodes = ReadSizeValue(limitsJson, "maxKnownNodes", ReadSizeValue(legacyLimitsJson, "maxKnownNodes", config->runtimeMaxKnownNodes));
  config->runtimeJoinCandidateCount = ReadU32Value(joinJson, "maxCandidates", ReadU32Value(legacyJoinJson, "maxCandidates", config->runtimeJoinCandidateCount));
  config->runtimeJoinMaxHops = ReadU32Value(joinJson, "maxHops", ReadU32Value(legacyJoinJson, "maxHops", config->runtimeJoinMaxHops));
  config->runtimeNeighborRefreshMs = ReadU32Value(periodsJson, "neighborRefresh", ReadU32Value(legacyPeriodsJson, "neighborRefresh", config->runtimeNeighborRefreshMs));
  config->runtimeTopologyBroadcastMs = ReadU32Value(periodsJson, "topologyBroadcast", ReadU32Value(legacyPeriodsJson, "topologyBroadcast", config->runtimeTopologyBroadcastMs));
  config->runtimePlayerBroadcastMs = ReadU32Value(periodsJson, "playerBroadcast", ReadU32Value(legacyPeriodsJson, "playerBroadcast", config->runtimePlayerBroadcastMs));

  config->simulatorFrames = ReadIntValue(root, "frames", ReadIntValue(simulationJson, "frames", config->simulatorFrames));
  config->simulatorFrameTime = ReadFloatValue(root, "frameTime", ReadFloatValue(simulationJson, "frameTime", config->simulatorFrameTime));
  config->simulatorSleepMs = ReadIntValue(root, "sleepMs", ReadIntValue(simulationJson, "sleepMs", config->simulatorSleepMs));
  config->relayTicks = ReadIntValue(root, "ticks", ReadIntValue(serviceJson, "ticks", config->relayTicks));

  config->runtimeReceiveTimeoutMs = ClampU32Value(config->runtimeReceiveTimeoutMs, 1, MAX_RUNTIME_RECEIVE_TIMEOUT_MS);
  config->runtimeMaxNodeConnections = ClampSizeValue(config->runtimeMaxNodeConnections, 1, MAX_RUNTIME_NODE_CONNECTIONS);
  config->runtimeMaxKnownNodes = ClampSizeValue(config->runtimeMaxKnownNodes, config->runtimeMaxNodeConnections, MAX_RUNTIME_KNOWN_NODES);
  config->runtimeJoinCandidateCount = ClampU32Value(config->runtimeJoinCandidateCount, 1, MAX_RUNTIME_JOIN_CANDIDATE_COUNT);
  config->runtimeJoinMaxHops = ClampU32Value(config->runtimeJoinMaxHops, 1, MAX_RUNTIME_JOIN_MAX_HOPS);
  config->runtimeNeighborRefreshMs = ClampU32Value(config->runtimeNeighborRefreshMs, 1, MAX_RUNTIME_PERIOD_MS);
  config->runtimeTopologyBroadcastMs = ClampU32Value(config->runtimeTopologyBroadcastMs, 1, MAX_RUNTIME_PERIOD_MS);
  config->runtimePlayerBroadcastMs = ClampU32Value(config->runtimePlayerBroadcastMs, 1, MAX_RUNTIME_PERIOD_MS);
  config->simulatorFrameTime = std::max(0.0001f, config->simulatorFrameTime);
  config->simulatorSleepMs = std::max(0, config->simulatorSleepMs);
  config->relayTicks = std::max(0, config->relayTicks);
  return true;
}
