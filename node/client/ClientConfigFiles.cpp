#include "ClientConfigFiles.h"

#include <algorithm>
#include <filesystem>
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

static const nlohmann::json& ReadObjectOrEmpty(const nlohmann::json& json, const char* key)
{
  static const nlohmann::json empty = nlohmann::json::object();
  if (!json.is_object() || key == nullptr || !json.contains(key) || !json[key].is_object()) return empty;
  return json[key];
}

static std::string NormalizeRealmName(const std::string& value)
{
  return value.empty() ? "unnamed" : value;
}

bool LoadClientFilesConfig(const std::string& configPath, ClientFilesConfig* config, std::string* errorMessage)
{
  if (config == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "client config output was null";
    return false;
  }

  nlohmann::json root = {};
  if (!LoadJsonFile(configPath, &root, errorMessage)) return false;

  config->configPath = configPath;
  const nlohmann::json& clientJson = ReadObjectOrEmpty(root, "client");
  config->masterVolume = ReadFloatValue(root, "masterVolume", ReadFloatValue(clientJson, "masterVolume", config->masterVolume));
  config->mouseSensitivity = ReadFloatValue(root, "mouseSensitivity", ReadFloatValue(clientJson, "mouseSensitivity", config->mouseSensitivity));
  config->invertMouseY = ReadBoolValue(root, "invertMouseY", ReadBoolValue(clientJson, "invertMouseY", config->invertMouseY));
  config->showFps = ReadBoolValue(root, "showFps", ReadBoolValue(clientJson, "showFps", config->showFps));
  return true;
}

bool SaveClientFilesConfig(const ClientFilesConfig& config, std::string* errorMessage)
{
  nlohmann::json root = {};
  std::string ignoredError = {};
  if (std::filesystem::exists(config.configPath) && !LoadJsonFile(config.configPath, &root, &ignoredError))
  {
    if (errorMessage != nullptr) *errorMessage = ignoredError;
    return false;
  }

  root["masterVolume"] = config.masterVolume;
  root["mouseSensitivity"] = config.mouseSensitivity;
  root["invertMouseY"] = config.invertMouseY;
  root["showFps"] = config.showFps;

  if (root.contains("realm")) root.erase("realm");
  if (root.contains("client")) root.erase("client");

  if (root.contains("runtime") && root["runtime"].is_object())
  {
    nlohmann::json& runtimeJson = root["runtime"];
    if (runtimeJson.contains("jumpNodeIndex")) runtimeJson.erase("jumpNodeIndex");
    if (runtimeJson.contains("join") && runtimeJson["join"].is_object())
    {
      nlohmann::json& joinJson = runtimeJson["join"];
      if (joinJson.contains("targetPosition")) joinJson.erase("targetPosition");
      if (joinJson.empty()) runtimeJson.erase("join");
    }
    if (runtimeJson.empty()) root.erase("runtime");
  }

  std::ofstream stream(config.configPath, std::ios::trunc);
  if (!stream.is_open())
  {
    if (errorMessage != nullptr) *errorMessage = "failed to write " + config.configPath;
    return false;
  }

  stream << root.dump(2) << '\n';
  return true;
}

std::vector<std::string> DiscoverClientRealmDirectories()
{
  std::vector<std::string> realms = {};
  const std::filesystem::path realmsRoot = "realms";
  if (!std::filesystem::exists(realmsRoot) || !std::filesystem::is_directory(realmsRoot)) return realms;

  for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(realmsRoot))
  {
    if (!entry.is_directory()) continue;
    const std::filesystem::path realmJson = entry.path() / "realm.json";
    const std::filesystem::path jumpNodesJson = entry.path() / "jump_nodes.json";
    if (!std::filesystem::exists(realmJson) || !std::filesystem::exists(jumpNodesJson)) continue;
    realms.push_back(entry.path().generic_string());
  }

  std::sort(realms.begin(), realms.end());
  return realms;
}

bool LoadClientRealmState(const std::string& realmDirectory, ClientRealmState* realmState, std::string* errorMessage)
{
  if (realmState == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "realm state output was null";
    return false;
  }

  nlohmann::json realmJson = {};
  if (!LoadJsonFile(realmDirectory + "/realm.json", &realmJson, errorMessage)) return false;

  nlohmann::json jumpNodesJson = {};
  if (!LoadJsonFile(realmDirectory + "/jump_nodes.json", &jumpNodesJson, errorMessage)) return false;

  realmState->directory = realmDirectory;
  realmState->realmName = NormalizeRealmName(ReadStringValue(realmJson, "name", realmDirectory));
  realmState->jumpNodes.clear();

  const nlohmann::json jumpNodesArray = jumpNodesJson.value("jumpNodes", nlohmann::json::array());
  if (!jumpNodesArray.is_array())
  {
    if (errorMessage != nullptr) *errorMessage = "jumpNodes was not an array in " + realmDirectory + "/jump_nodes.json";
    return false;
  }

  for (const nlohmann::json& jumpNodeJson : jumpNodesArray)
  {
    ClientJumpNodeState jumpNode = {};
    jumpNode.host = ReadStringValue(jumpNodeJson, "host", jumpNode.host);
    jumpNode.port = ReadIntValue(jumpNodeJson, "port", jumpNode.port);
    jumpNode.label = ReadStringValue(jumpNodeJson, "label", jumpNode.label);
    if (jumpNode.host.empty() || jumpNode.port <= 0) continue;
    realmState->jumpNodes.push_back(jumpNode);
  }

  return true;
}
