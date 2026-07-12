#include "RealmConfigFiles.h"
#include "../runtime/ProtocolVersion.h"

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

  *json = nlohmann::json::parse(stream, nullptr, false, true);
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

static RuntimePeerAddress ReadPeerAddressValue(const nlohmann::json& json, const RuntimePeerAddress& fallback)
{
  RuntimePeerAddress peerAddress = fallback;
  if (!json.is_object()) return peerAddress;
  peerAddress.host = ReadStringValue(json, "host", peerAddress.host);
  peerAddress.port = ReadIntValue(json, "port", peerAddress.port);
  return peerAddress;
}

bool LoadRealmConfigFiles(const std::string& realmDirectory, RealmConfigFiles* realmFiles, std::string* errorMessage)
{
  if (realmFiles == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "realm config output was null";
    return false;
  }

  nlohmann::json realmJson = {};
  if (!LoadJsonFile(realmDirectory + "/realm.json", &realmJson, errorMessage)) return false;

  nlohmann::json jumpNodesJson = {};
  if (!LoadJsonFile(realmDirectory + "/jump_nodes.json", &jumpNodesJson, errorMessage)) return false;

  realmFiles->directory = realmDirectory;
  realmFiles->realmName = ReadStringValue(realmJson, "name", realmDirectory);

  const nlohmann::json blockchainJson = realmJson.value("blockchain", nlohmann::json::object());
  realmFiles->blockchainConfig.protocolVersion = ReadU32Value(
      blockchainJson,
      "protocolVersion",
      realmFiles->blockchainConfig.protocolVersion);
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

  if (realmFiles->blockchainConfig.protocolVersion != kBlockchainProtocolVersion)
  {
    if (errorMessage != nullptr)
    {
      *errorMessage =
          realmDirectory + "/realm.json protocolVersion "
          + std::to_string(realmFiles->blockchainConfig.protocolVersion)
          + " does not match orchestration protocol version "
          + std::to_string(kBlockchainProtocolVersion);
    }
    return false;
  }

  realmFiles->jumpNodes.clear();
  const nlohmann::json jumpNodesArray = jumpNodesJson.value("jumpNodes", nlohmann::json::array());
  if (!jumpNodesArray.is_array())
  {
    if (errorMessage != nullptr) *errorMessage = "jumpNodes was not an array in " + realmDirectory + "/jump_nodes.json";
    return false;
  }

  for (const nlohmann::json& jumpNodeJson : jumpNodesArray)
  {
    RealmJumpNodeState jumpNode = {};
    jumpNode.peerAddress = ReadPeerAddressValue(jumpNodeJson, jumpNode.peerAddress);
    jumpNode.label = ReadStringValue(jumpNodeJson, "label", jumpNode.label);
    if (jumpNode.peerAddress.host.empty() || jumpNode.peerAddress.port <= 0) continue;
    realmFiles->jumpNodes.push_back(std::move(jumpNode));
  }

  return true;
}

RuntimeRealmState BuildRuntimeRealmState(Blockchain& blockchain, const BlockchainConfig& blockchainConfig)
{
  RuntimeRealmState realmState = {};
  realmState.runtimeProtocolVersion = kRuntimeProtocolVersion;
  const std::string chainId = blockchain.GetRpcClient().EthChainId();
  realmState.chainId = chainId.empty() ? "unavailable" : chainId;
  realmState.blockchainConfig = blockchainConfig;
  realmState.globalParams = blockchain.GetGlobalParams().GetState();
  return realmState;
}
