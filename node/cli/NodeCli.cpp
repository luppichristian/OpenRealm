#include "NodeCli.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#if defined(_WIN32)
#  include <io.h>
#else
#  include <unistd.h>
#endif

static bool IsInputInteractive()
{
#if defined(_WIN32)
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(fileno(stdin)) != 0;
#endif
}

static bool HasArgument(int argc, char** argv, const char* argument)
{
  if (argument == nullptr) return false;
  for (int i = 1; i < argc; ++i)
  {
    if (argv[i] != nullptr && std::string(argv[i]) == argument) return true;
  }
  return false;
}

bool ShouldRunNodeCli(int argc, char** argv)
{
  if (HasArgument(argc, argv, "--no-cli")) return false;
  if (HasArgument(argc, argv, "--smoke")) return false;
  return IsInputInteractive();
}

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

static bool SaveJsonFile(const std::string& path, const nlohmann::json& json, std::string* errorMessage)
{
  std::ofstream stream(path, std::ios::trunc);
  if (!stream.is_open())
  {
    if (errorMessage != nullptr) *errorMessage = "failed to write " + path;
    return false;
  }

  stream << json.dump(2) << '\n';
  return true;
}

static std::string ReadLine(const std::string& prompt)
{
  std::cout << prompt;
  std::string value = {};
  std::getline(std::cin, value);
  return value;
}

static std::string Trim(const std::string& value)
{
  size_t begin = 0;
  while (begin < value.size() && std::isspace((unsigned char)value[begin])) begin += 1;

  size_t end = value.size();
  while (end > begin && std::isspace((unsigned char)value[end - 1])) end -= 1;
  return value.substr(begin, end - begin);
}

static int ReadMenuChoice(int fallback)
{
  const std::string value = Trim(ReadLine("> "));
  if (value.empty()) return fallback;

  char* end = nullptr;
  const long parsed = std::strtol(value.c_str(), &end, 10);
  if (end == value.c_str() || (end != nullptr && *end != '\0')) return fallback;
  return (int)parsed;
}

static nlohmann::json& EnsureObject(nlohmann::json& root, const char* key)
{
  if (!root.contains(key) || !root[key].is_object()) root[key] = nlohmann::json::object();
  return root[key];
}

static nlohmann::json& EnsureAddress(nlohmann::json& object)
{
  if (!object.contains("bindAddress") || !object["bindAddress"].is_object()) object["bindAddress"] = nlohmann::json::object();
  return object["bindAddress"];
}

static std::string JsonString(const nlohmann::json& json, const char* key, const std::string& fallback)
{
  if (!json.is_object() || key == nullptr || !json.contains(key) || !json[key].is_string()) return fallback;
  return json[key].get<std::string>();
}

static int JsonInt(const nlohmann::json& json, const char* key, int fallback)
{
  if (!json.is_object() || key == nullptr || !json.contains(key) || !json[key].is_number_integer()) return fallback;
  return json[key].get<int>();
}

static bool JsonBool(const nlohmann::json& json, const char* key, bool fallback)
{
  if (!json.is_object() || key == nullptr || !json.contains(key) || !json[key].is_boolean()) return fallback;
  return json[key].get<bool>();
}

static float JsonFloat(const nlohmann::json& json, const char* key, float fallback)
{
  if (!json.is_object() || key == nullptr || !json.contains(key) || !json[key].is_number()) return fallback;
  return json[key].get<float>();
}

static void SetStringIfEntered(nlohmann::json& json, const char* key, const std::string& prompt, const std::string& current)
{
  const std::string value = Trim(ReadLine(prompt + std::string(" [") + current + "]: "));
  if (!value.empty()) json[key] = value;
}

static void SetIntIfEntered(nlohmann::json& json, const char* key, const std::string& prompt, int current)
{
  const std::string value = Trim(ReadLine(prompt + std::string(" [") + std::to_string(current) + "]: "));
  if (value.empty()) return;

  char* end = nullptr;
  const long parsed = std::strtol(value.c_str(), &end, 10);
  if (end == value.c_str() || (end != nullptr && *end != '\0'))
  {
    std::cout << "Invalid number; keeping previous value.\n";
    return;
  }
  json[key] = (int)parsed;
}

static void SetFloatIfEntered(nlohmann::json& json, const char* key, const std::string& prompt, float current)
{
  const std::string value = Trim(ReadLine(prompt + std::string(" [") + std::to_string(current) + "]: "));
  if (value.empty()) return;

  char* end = nullptr;
  const float parsed = std::strtof(value.c_str(), &end);
  if (end == value.c_str() || (end != nullptr && *end != '\0'))
  {
    std::cout << "Invalid number; keeping previous value.\n";
    return;
  }
  json[key] = parsed;
}

static void SetBoolIfEntered(nlohmann::json& json, const char* key, const std::string& prompt, bool current)
{
  const std::string currentText = current ? "true" : "false";
  std::string value = Trim(ReadLine(prompt + std::string(" [") + currentText + "]: "));
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                 { return (char)std::tolower(c); });
  if (value.empty()) return;

  if (value == "true" || value == "t" || value == "yes" || value == "y" || value == "1")
  {
    json[key] = true;
    return;
  }
  if (value == "false" || value == "f" || value == "no" || value == "n" || value == "0")
  {
    json[key] = false;
    return;
  }

  std::cout << "Invalid boolean; keeping previous value.\n";
}

static std::vector<std::string> DiscoverRealmDirectories()
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

static void PrintNodeValues(const nlohmann::json& root, NodeCliRole role)
{
  const std::string roleKey = role == NodeCliRole::Relay ? "relay" : "simulator";
  const nlohmann::json roleJson = root.value(roleKey, nlohmann::json::object());
  const nlohmann::json bindAddress = roleJson.value("bindAddress", nlohmann::json::object());
  const nlohmann::json wallet = root.value("wallet", nlohmann::json::object());

  std::cout << "\nCurrent node config\n";
  std::cout << "- realm: " << JsonString(root, "realm", "realms/test") << "\n";
  std::cout << "- wallet account: " << JsonString(wallet, "accountAddress", "") << "\n";
  std::cout << "- wallet runtime signer: " << JsonString(wallet, "runtimeSignerAddress", "") << "\n";
  std::cout << "- " << roleKey << " bind: " << JsonString(bindAddress, "host", "127.0.0.1") << ":" << JsonInt(bindAddress, "port", 0) << "\n";
  std::cout << "- " << roleKey << " node id: " << JsonInt(roleJson, "nodeId", 0) << "\n";

  if (role == NodeCliRole::Relay)
  {
    std::cout << "- relay timeout ms: " << JsonInt(roleJson, "receiveTimeoutMs", 100) << "\n";
    std::cout << "- relay ticks: " << JsonInt(roleJson, "ticks", 0) << "\n";
  }
  else
  {
    std::cout << "- simulator jump node index: " << JsonInt(roleJson, "jumpNodeIndex", 0) << "\n";
    std::cout << "- simulator interest: (" << JsonInt(roleJson, "interestChunkX", 0) << ", " << JsonInt(roleJson, "interestChunkY", 0) << ") r=" << JsonInt(roleJson, "interestRadius", 1) << "\n";
    std::cout << "- simulator frames: " << JsonInt(roleJson, "frames", 0) << "\n";
    std::cout << "- simulator frame time: " << JsonFloat(roleJson, "frameTime", 1.0f / 60.0f) << "\n";
    std::cout << "- simulator sleep ms: " << JsonInt(roleJson, "sleepMs", 16) << "\n";
    std::cout << "- runtime enabled: " << (JsonBool(roleJson, "runtimeEnabled", false) ? "true" : "false") << "\n";
    std::cout << "- emit place event: " << (JsonBool(roleJson, "emitPlaceEvent", false) ? "true" : "false") << "\n";
    std::cout << "- place voxel value: " << JsonInt(roleJson, "placeVoxelValue", 255) << "\n";
    std::cout << "- receive timeout ms: " << JsonInt(roleJson, "receiveTimeoutMs", 1000) << "\n";
  }
  std::cout << "\n";
}

static void PrintRealmValues(const nlohmann::json& root)
{
  const std::string realmDirectory = JsonString(root, "realm", "realms/test");
  nlohmann::json realm = {};
  nlohmann::json jumpNodes = {};
  std::string error = {};

  std::cout << "\nCurrent realm config\n";
  std::cout << "- directory: " << realmDirectory << "\n";
  if (!LoadJsonFile(realmDirectory + "/realm.json", &realm, &error))
  {
    std::cout << "- realm.json: " << error << "\n\n";
    return;
  }

  const nlohmann::json blockchain = realm.value("blockchain", nlohmann::json::object());
  std::cout << "- name: " << JsonString(realm, "name", realmDirectory) << "\n";
  std::cout << "- rpc url: " << JsonString(blockchain, "rpcUrl", "") << "\n";
  std::cout << "- global params: " << JsonString(blockchain, "globalParamsAddress", "") << "\n";
  std::cout << "- player registry: " << JsonString(blockchain, "playerRegistryAddress", "") << "\n";
  std::cout << "- chunk claims: " << JsonString(blockchain, "chunkClaimsAddress", "") << "\n";
  std::cout << "- marketplace: " << JsonString(blockchain, "marketplaceAddress", "") << "\n";

  if (LoadJsonFile(realmDirectory + "/jump_nodes.json", &jumpNodes, &error))
  {
    const nlohmann::json nodes = jumpNodes.value("jumpNodes", nlohmann::json::array());
    std::cout << "- jump nodes: " << nodes.size() << "\n";
    for (size_t i = 0; i < nodes.size(); ++i)
    {
      const nlohmann::json address = nodes[i];
      std::cout << "  " << i << ": " << JsonString(address, "host", "") << ":" << JsonInt(address, "port", 0) << " " << JsonString(address, "label", "") << "\n";
    }
  }
  std::cout << "\n";
}

static void PickRealm(nlohmann::json& root)
{
  const std::vector<std::string> realms = DiscoverRealmDirectories();
  if (realms.empty())
  {
    std::cout << "No realm directories found under realms/.\n";
    return;
  }

  std::cout << "\nAvailable realms\n";
  for (size_t i = 0; i < realms.size(); ++i)
  {
    std::cout << i + 1 << ") " << realms[i] << "\n";
  }

  const int choice = ReadMenuChoice(0);
  if (choice <= 0 || (size_t)choice > realms.size())
  {
    std::cout << "Invalid realm choice.\n";
    return;
  }

  root["realm"] = realms[(size_t)choice - 1];
  std::cout << "Selected realm: " << root["realm"].get<std::string>() << "\n";
}

static void EditWalletValues(nlohmann::json& root)
{
  nlohmann::json& wallet = EnsureObject(root, "wallet");
  SetStringIfEntered(wallet, "accountAddress", "Wallet account address", JsonString(wallet, "accountAddress", ""));
  SetStringIfEntered(wallet, "runtimeSignerAddress", "Runtime signer address", JsonString(wallet, "runtimeSignerAddress", ""));
}

static void EditRelayValues(nlohmann::json& root)
{
  nlohmann::json& relay = EnsureObject(root, "relay");
  nlohmann::json& bindAddress = EnsureAddress(relay);
  SetStringIfEntered(bindAddress, "host", "Relay bind host", JsonString(bindAddress, "host", "127.0.0.1"));
  SetIntIfEntered(bindAddress, "port", "Relay bind port", JsonInt(bindAddress, "port", 46001));
  SetIntIfEntered(relay, "nodeId", "Relay node id", JsonInt(relay, "nodeId", 9001));
  SetIntIfEntered(relay, "receiveTimeoutMs", "Relay receive timeout ms", JsonInt(relay, "receiveTimeoutMs", 100));
  SetIntIfEntered(relay, "ticks", "Relay ticks (0 = forever)", JsonInt(relay, "ticks", 0));
}

static void EditSimulatorValues(nlohmann::json& root)
{
  nlohmann::json& simulator = EnsureObject(root, "simulator");
  nlohmann::json& bindAddress = EnsureAddress(simulator);
  SetStringIfEntered(bindAddress, "host", "Simulator bind host", JsonString(bindAddress, "host", "127.0.0.1"));
  SetIntIfEntered(bindAddress, "port", "Simulator bind port", JsonInt(bindAddress, "port", 46010));
  SetIntIfEntered(simulator, "nodeId", "Simulator node id", JsonInt(simulator, "nodeId", 7001));
  SetIntIfEntered(simulator, "jumpNodeIndex", "Jump node index", JsonInt(simulator, "jumpNodeIndex", 0));
  SetIntIfEntered(simulator, "interestChunkX", "Interest chunk X", JsonInt(simulator, "interestChunkX", 0));
  SetIntIfEntered(simulator, "interestChunkY", "Interest chunk Y", JsonInt(simulator, "interestChunkY", 0));
  SetIntIfEntered(simulator, "interestRadius", "Interest radius", JsonInt(simulator, "interestRadius", 1));
  SetIntIfEntered(simulator, "frames", "Frames (0 = forever)", JsonInt(simulator, "frames", 0));
  SetFloatIfEntered(simulator, "frameTime", "Frame time", JsonFloat(simulator, "frameTime", 1.0f / 60.0f));
  SetIntIfEntered(simulator, "sleepMs", "Sleep ms", JsonInt(simulator, "sleepMs", 16));
  SetBoolIfEntered(simulator, "runtimeEnabled", "Runtime enabled", JsonBool(simulator, "runtimeEnabled", false));
  SetBoolIfEntered(simulator, "emitPlaceEvent", "Emit place event", JsonBool(simulator, "emitPlaceEvent", false));
  SetIntIfEntered(simulator, "placeVoxelValue", "Place voxel value", JsonInt(simulator, "placeVoxelValue", 255));
  SetIntIfEntered(simulator, "receiveTimeoutMs", "Receive timeout ms", JsonInt(simulator, "receiveTimeoutMs", 1000));
}

static void PrintMenu(NodeCliRole role)
{
  std::cout << "OpenRealm " << (role == NodeCliRole::Relay ? "relay" : "simulator") << " configuration\n";
  std::cout << "1) View node values\n";
  std::cout << "2) View realm values\n";
  std::cout << "3) Pick realm\n";
  std::cout << "4) Edit " << (role == NodeCliRole::Relay ? "relay" : "simulator") << " values\n";
  std::cout << "5) Edit wallet values\n";
  std::cout << "6) Save and launch\n";
  std::cout << "7) Launch without saving\n";
  std::cout << "8) Exit\n";
}

bool RunNodeCli(const NodeCliOptions& options)
{
  nlohmann::json root = {};
  std::string error = {};
  if (!LoadJsonFile(options.configPath, &root, &error))
  {
    std::cout << "OpenRealm config CLI failed: " << error << "\n";
    return false;
  }

  bool dirty = false;
  for (;;)
  {
    PrintMenu(options.role);
    const int choice = ReadMenuChoice(0);
    switch (choice)
    {
      case 1: PrintNodeValues(root, options.role); break;
      case 2: PrintRealmValues(root); break;
      case 3:
        PickRealm(root);
        dirty = true;
        break;
      case 4:
        if (options.role == NodeCliRole::Relay) EditRelayValues(root);
        else
          EditSimulatorValues(root);
        dirty = true;
        break;
      case 5:
        EditWalletValues(root);
        dirty = true;
        break;
      case 6:
        if (dirty && !SaveJsonFile(options.configPath, root, &error))
        {
          std::cout << "Failed to save config: " << error << "\n";
          break;
        }
        return true;
      case 7:  return true;
      case 8:  return false;
      default: std::cout << "Unknown choice.\n"; break;
    }
  }
}
