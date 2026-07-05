#include "NodeConfigFiles.h"

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
  config->wallet.Connect(
      ReadStringValue(walletJson, "accountAddress", config->wallet.GetAccountAddress()),
      ReadStringValue(walletJson, "runtimeSignerAddress", config->wallet.GetRuntimeSignerAddress()));

  const nlohmann::json simulatorJson = root.value("simulator", nlohmann::json::object());
  config->simulatorBindAddress = ReadPeerAddressValue(
      simulatorJson.value("bindAddress", nlohmann::json::object()),
      config->simulatorBindAddress);
  config->simulatorJumpNodeIndex = ReadIntValue(simulatorJson, "jumpNodeIndex", config->simulatorJumpNodeIndex);
  config->simulatorNodeId = ReadU32Value(simulatorJson, "nodeId", config->simulatorNodeId);
  config->simulatorInterestChunkX = ReadIntValue(simulatorJson, "interestChunkX", config->simulatorInterestChunkX);
  config->simulatorInterestChunkY = ReadIntValue(simulatorJson, "interestChunkY", config->simulatorInterestChunkY);
  config->simulatorInterestRadius = ReadU32Value(simulatorJson, "interestRadius", config->simulatorInterestRadius);
  config->simulatorPlaceVoxelValue = (uint8_t)ReadIntValue(simulatorJson, "placeVoxelValue", (int)config->simulatorPlaceVoxelValue);
  config->simulatorReceiveTimeoutMs = ReadU32Value(simulatorJson, "receiveTimeoutMs", config->simulatorReceiveTimeoutMs);
  config->simulatorFrames = ReadIntValue(simulatorJson, "frames", config->simulatorFrames);
  config->simulatorFrameTime = ReadFloatValue(simulatorJson, "frameTime", config->simulatorFrameTime);
  config->simulatorSleepMs = ReadIntValue(simulatorJson, "sleepMs", config->simulatorSleepMs);
  config->simulatorRuntimeEnabled = ReadBoolValue(simulatorJson, "runtimeEnabled", config->simulatorRuntimeEnabled);
  config->simulatorEmitPlaceEvent = ReadBoolValue(simulatorJson, "emitPlaceEvent", config->simulatorEmitPlaceEvent);

  const nlohmann::json relayJson = root.value("relay", nlohmann::json::object());
  config->relayBindAddress = ReadPeerAddressValue(
      relayJson.value("bindAddress", nlohmann::json::object()),
      config->relayBindAddress);
  config->relayNodeId = ReadU32Value(relayJson, "nodeId", config->relayNodeId);
  config->relayReceiveTimeoutMs = ReadU32Value(relayJson, "receiveTimeoutMs", config->relayReceiveTimeoutMs);
  config->relayTicks = ReadIntValue(relayJson, "ticks", config->relayTicks);
  return true;
}
