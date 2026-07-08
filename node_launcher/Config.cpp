#include "Config.h"

#include <fstream>

#include "LauncherUtilities.h"

namespace
{
nlohmann::json& EnsureObject(nlohmann::json& root, const char* key)
{
  if (!root.contains(key) || !root[key].is_object()) root[key] = nlohmann::json::object();
  return root[key];
}

nlohmann::json& EnsureRuntimeObject(nlohmann::json& root)
{
  return EnsureObject(root, "runtime");
}

nlohmann::json& EnsureSimulationObject(nlohmann::json& root)
{
  return EnsureObject(root, "simulation");
}

nlohmann::json& EnsureServiceObject(nlohmann::json& root)
{
  return EnsureObject(root, "service");
}

nlohmann::json& EnsureWalletObject(nlohmann::json& root)
{
  return EnsureObject(root, "wallet");
}

nlohmann::json& EnsureRuntimeInterestObject(nlohmann::json& root)
{
  return EnsureObject(EnsureRuntimeObject(root), "interest");
}

nlohmann::json& EnsureBindAddressObject(nlohmann::json& root)
{
  return EnsureObject(EnsureRuntimeObject(root), "bindAddress");
}
}

bool LoadJsonFile(const std::filesystem::path& path, nlohmann::json* json, std::string* errorMessage)
{
  if (json == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "json output was null";
    return false;
  }

  std::ifstream stream(path);
  if (!stream.is_open())
  {
    if (errorMessage != nullptr) *errorMessage = "failed to open " + FormatPath(path);
    return false;
  }

  *json = nlohmann::json::parse(stream, nullptr, false);
  if (json->is_discarded())
  {
    if (errorMessage != nullptr) *errorMessage = "failed to parse " + FormatPath(path);
    return false;
  }

  return true;
}

bool SaveJsonFile(const std::filesystem::path& path, const nlohmann::json& json, std::string* errorMessage)
{
  std::error_code error = {};
  std::filesystem::create_directories(path.parent_path(), error);

  std::ofstream stream(path, std::ios::trunc);
  if (!stream.is_open())
  {
    if (errorMessage != nullptr) *errorMessage = "failed to write " + FormatPath(path);
    return false;
  }

  stream << json.dump(2) << '\n';
  return true;
}

void NormalizeConfigSchema(nlohmann::json* root)
{
  if (root == nullptr || !root->is_object()) return;
  EnsureWalletObject(*root);
  EnsureBindAddressObject(*root);
  EnsureRuntimeInterestObject(*root);
  EnsureSimulationObject(*root);
  EnsureServiceObject(*root);
}

std::filesystem::path ResolveRealmDirectory(const std::filesystem::path& repoRoot, const std::string& realmArgument)
{
  const std::string trimmed = Trim(realmArgument);
  if (trimmed.empty()) return {};

  const std::filesystem::path directPath(trimmed);
  if (directPath.is_absolute()) return directPath;

  const std::filesystem::path repoRelative = repoRoot / directPath;
  if (std::filesystem::exists(repoRelative)) return repoRelative;

  const std::filesystem::path realmRelative = repoRoot / "realms" / directPath;
  if (std::filesystem::exists(realmRelative)) return realmRelative;

  return realmRelative;
}

bool ValidateRealmDirectory(const std::filesystem::path& realmDir, std::string* errorMessage)
{
  if (!std::filesystem::exists(realmDir) || !std::filesystem::is_directory(realmDir))
  {
    if (errorMessage != nullptr) *errorMessage = "realm directory not found: " + FormatPath(realmDir);
    return false;
  }

  const std::filesystem::path realmJson = realmDir / "realm.json";
  const std::filesystem::path jumpNodesJson = realmDir / "jump_nodes.json";
  if (!std::filesystem::exists(realmJson))
  {
    if (errorMessage != nullptr) *errorMessage = "realm.json not found in " + FormatPath(realmDir);
    return false;
  }

  if (!std::filesystem::exists(jumpNodesJson))
  {
    if (errorMessage != nullptr) *errorMessage = "jump_nodes.json not found in " + FormatPath(realmDir);
    return false;
  }

  return true;
}

bool CreateSessionRealm(
    const std::filesystem::path& sourceRealmDir,
    const std::filesystem::path& sessionRealmDir,
    const std::vector<int>& relayPorts,
    std::string* errorMessage)
{
  std::error_code error = {};
  std::filesystem::create_directories(sessionRealmDir, error);
  if (error)
  {
    if (errorMessage != nullptr) *errorMessage = "failed to create session realm directory: " + FormatPath(sessionRealmDir);
    return false;
  }

  nlohmann::json realmJson = {};
  if (!LoadJsonFile(sourceRealmDir / "realm.json", &realmJson, errorMessage)) return false;
  if (!SaveJsonFile(sessionRealmDir / "realm.json", realmJson, errorMessage)) return false;

  if (relayPorts.empty())
  {
    nlohmann::json jumpNodesJson = {};
    if (!LoadJsonFile(sourceRealmDir / "jump_nodes.json", &jumpNodesJson, errorMessage)) return false;
    return SaveJsonFile(sessionRealmDir / "jump_nodes.json", jumpNodesJson, errorMessage);
  }

  nlohmann::json jumpNodesJson = nlohmann::json::object();
  jumpNodesJson["jumpNodes"] = nlohmann::json::array();
  for (size_t i = 0; i < relayPorts.size(); ++i)
  {
    jumpNodesJson["jumpNodes"].push_back({
        { "host",                               "127.0.0.1"},
        { "port",                             relayPorts[i]},
        {"label", "launcher-relay-" + std::to_string(i + 1)},
    });
  }

  return SaveJsonFile(sessionRealmDir / "jump_nodes.json", jumpNodesJson, errorMessage);
}

bool BuildRelayConfig(
    const nlohmann::json& baseConfig,
    const LaunchOptions& options,
    const std::filesystem::path& realmDir,
    int relayIndex,
    const std::filesystem::path& outputPath,
    std::string* errorMessage)
{
  nlohmann::json config = baseConfig;
  NormalizeConfigSchema(&config);

  const int bindPort = options.relayBasePort + relayIndex;
  const uint32_t nodeId = options.relayBaseNodeId + (uint32_t)relayIndex;

  config["realm"] = FormatPath(realmDir);
  nlohmann::json& bindAddress = EnsureBindAddressObject(config);
  bindAddress["host"] = "127.0.0.1";
  bindAddress["port"] = bindPort;

  nlohmann::json& runtimeJson = EnsureRuntimeObject(config);
  runtimeJson["nodeId"] = nodeId;
  runtimeJson["enabled"] = true;

  nlohmann::json& serviceJson = EnsureServiceObject(config);
  serviceJson["ticks"] = options.relayTicks;

  return SaveJsonFile(outputPath, config, errorMessage);
}

bool BuildSimulatorConfig(
    const nlohmann::json& baseConfig,
    const LaunchOptions& options,
    const std::filesystem::path& realmDir,
    int simulatorIndex,
    int launchedRelayCount,
    const std::filesystem::path& outputPath,
    std::string* errorMessage)
{
  nlohmann::json config = baseConfig;
  NormalizeConfigSchema(&config);

  const int bindPort = options.simulatorBasePort + simulatorIndex;
  const uint32_t nodeId = options.simulatorBaseNodeId + (uint32_t)simulatorIndex;
  const std::pair<int, int> interest = BuildSimulatorInterestCoords(simulatorIndex);

  config["realm"] = FormatPath(realmDir);
  nlohmann::json& bindAddress = EnsureBindAddressObject(config);
  bindAddress["host"] = "127.0.0.1";
  bindAddress["port"] = bindPort;

  nlohmann::json& runtimeJson = EnsureRuntimeObject(config);
  runtimeJson["nodeId"] = nodeId;
  runtimeJson["enabled"] = true;
  runtimeJson["jumpNodeIndex"] = launchedRelayCount > 0 ? (simulatorIndex % launchedRelayCount) : 0;

  nlohmann::json& interestJson = EnsureRuntimeInterestObject(config);
  interestJson["chunkX"] = interest.first;
  interestJson["chunkY"] = interest.second;

  nlohmann::json& simulationJson = EnsureSimulationObject(config);
  simulationJson["frames"] = options.simulatorFrames;
  simulationJson["sleepMs"] = options.simulatorSleepMs;
  simulationJson["frameTime"] = options.simulatorFrameTime;
  simulationJson["emitPlaceEvent"] = options.emitPlaceEvent && simulatorIndex == 0;

  return SaveJsonFile(outputPath, config, errorMessage);
}
