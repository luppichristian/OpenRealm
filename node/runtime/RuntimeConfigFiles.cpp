#include "RuntimeConfigFiles.h"

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

static RuntimePeerAddress ReadPeerAddressValue(const nlohmann::json& json, const RuntimePeerAddress& fallback)
{
  RuntimePeerAddress peerAddress = fallback;
  if (!json.is_object()) return peerAddress;
  peerAddress.host = ReadStringValue(json, "host", peerAddress.host);
  peerAddress.port = ReadIntValue(json, "port", peerAddress.port);
  return peerAddress;
}

bool LoadRuntimeNodeFilesConfig(const std::string& configPath, RuntimeNodeFilesConfig* config, std::string* errorMessage)
{
  if (config == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "runtime node config output was null";
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
  config->simulatorFrameTime = ReadFloatValue(simulatorJson, "frameTime", config->simulatorFrameTime);
  config->simulatorSleepMs = ReadIntValue(simulatorJson, "sleepMs", config->simulatorSleepMs);

  const nlohmann::json relayJson = root.value("relay", nlohmann::json::object());
  config->relayBindAddress = ReadPeerAddressValue(
      relayJson.value("bindAddress", nlohmann::json::object()),
      config->relayBindAddress);
  config->relayNodeId = ReadU32Value(relayJson, "nodeId", config->relayNodeId);
  config->relayReceiveTimeoutMs = ReadU32Value(relayJson, "receiveTimeoutMs", config->relayReceiveTimeoutMs);
  config->relayTicks = ReadIntValue(relayJson, "ticks", config->relayTicks);
  return true;
}

bool LoadRuntimeRealmFiles(const std::string& realmDirectory, RuntimeRealmFiles* realmFiles, std::string* errorMessage)
{
  if (realmFiles == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "runtime realm output was null";
    return false;
  }

  nlohmann::json realmJson = {};
  if (!LoadJsonFile(realmDirectory + "/realm.json", &realmJson, errorMessage)) return false;

  nlohmann::json jumpNodesJson = {};
  if (!LoadJsonFile(realmDirectory + "/jump_nodes.json", &jumpNodesJson, errorMessage)) return false;

  realmFiles->directory = realmDirectory;
  realmFiles->realmName = ReadStringValue(realmJson, "name", realmDirectory);

  const nlohmann::json blockchainJson = realmJson.value("blockchain", nlohmann::json::object());
  realmFiles->blockchainConfig.rpcUrl = ReadStringValue(blockchainJson, "rpcUrl", realmFiles->blockchainConfig.rpcUrl);
  realmFiles->blockchainConfig.globalParamsAddress = ReadStringValue(
      blockchainJson,
      "globalParamsAddress",
      realmFiles->blockchainConfig.globalParamsAddress);
  realmFiles->blockchainConfig.playerRegistryAddress = ReadStringValue(
      blockchainJson,
      "playerRegistryAddress",
      realmFiles->blockchainConfig.playerRegistryAddress);
  realmFiles->blockchainConfig.chunkClaimsAddress = ReadStringValue(
      blockchainJson,
      "chunkClaimsAddress",
      realmFiles->blockchainConfig.chunkClaimsAddress);
  realmFiles->blockchainConfig.marketplaceAddress = ReadStringValue(
      blockchainJson,
      "marketplaceAddress",
      realmFiles->blockchainConfig.marketplaceAddress);
  realmFiles->blockchainConfig.connectionTimeoutSeconds = ReadIntValue(
      blockchainJson,
      "connectionTimeoutSeconds",
      realmFiles->blockchainConfig.connectionTimeoutSeconds);
  realmFiles->blockchainConfig.readTimeoutSeconds = ReadIntValue(
      blockchainJson,
      "readTimeoutSeconds",
      realmFiles->blockchainConfig.readTimeoutSeconds);
  realmFiles->blockchainConfig.writeTimeoutSeconds = ReadIntValue(
      blockchainJson,
      "writeTimeoutSeconds",
      realmFiles->blockchainConfig.writeTimeoutSeconds);

  realmFiles->jumpNodes.clear();
  const nlohmann::json jumpNodesArray = jumpNodesJson.value("jumpNodes", nlohmann::json::array());
  if (!jumpNodesArray.is_array())
  {
    if (errorMessage != nullptr) *errorMessage = "jumpNodes was not an array in " + realmDirectory + "/jump_nodes.json";
    return false;
  }

  for (const nlohmann::json& jumpNodeJson : jumpNodesArray)
  {
    RuntimeJumpNodeState jumpNode = {};
    jumpNode.peerAddress = ReadPeerAddressValue(jumpNodeJson, jumpNode.peerAddress);
    jumpNode.label = ReadStringValue(jumpNodeJson, "label", jumpNode.label);
    if (jumpNode.peerAddress.host.empty() || jumpNode.peerAddress.port <= 0) continue;
    realmFiles->jumpNodes.push_back(std::move(jumpNode));
  }

  return true;
}
