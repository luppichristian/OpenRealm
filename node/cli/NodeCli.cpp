#include "NodeCli.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "term.h"

#if defined(_WIN32)
#  include <io.h>
#else
#  include <unistd.h>
#endif

static constexpr int MIN_TERM_WIDTH = 90;
static constexpr int MIN_TERM_HEIGHT = 26;
static constexpr int FIELD_LABEL_WIDTH = 22;

static constexpr tcolor_t COLOR_BG = TRGBA(10, 14, 20, 255);
static constexpr tcolor_t COLOR_PANEL = TRGBA(20, 27, 38, 255);
static constexpr tcolor_t COLOR_PANEL_ALT = TRGBA(15, 21, 30, 255);
static constexpr tcolor_t COLOR_BORDER = TRGBA(70, 95, 125, 255);
static constexpr tcolor_t COLOR_HEADER = TRGBA(38, 84, 124, 255);
static constexpr tcolor_t COLOR_HEADER_ACCENT = TRGBA(96, 176, 255, 255);
static constexpr tcolor_t COLOR_TEXT = TRGBA(232, 239, 247, 255);
static constexpr tcolor_t COLOR_MUTED = TRGBA(150, 165, 186, 255);
static constexpr tcolor_t COLOR_SECTION = TRGBA(246, 206, 109, 255);
static constexpr tcolor_t COLOR_SELECTED_BG = TRGBA(44, 70, 102, 255);
static constexpr tcolor_t COLOR_SELECTED_BORDER = TRGBA(117, 189, 255, 255);
static constexpr tcolor_t COLOR_BOOL_TRUE = TRGBA(117, 214, 126, 255);
static constexpr tcolor_t COLOR_BOOL_FALSE = TRGBA(234, 125, 113, 255);
static constexpr tcolor_t COLOR_INFO = TRGBA(140, 195, 255, 255);
static constexpr tcolor_t COLOR_SUCCESS = TRGBA(127, 223, 156, 255);
static constexpr tcolor_t COLOR_WARNING = TRGBA(255, 208, 102, 255);
static constexpr tcolor_t COLOR_ERROR = TRGBA(255, 120, 120, 255);
static constexpr tcolor_t COLOR_OVERLAY = TRGBA(6, 9, 14, 255);

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

static std::string Trim(const std::string& value)
{
  size_t begin = 0;
  while (begin < value.size() && std::isspace((unsigned char)value[begin])) begin += 1;

  size_t end = value.size();
  while (end > begin && std::isspace((unsigned char)value[end - 1])) end -= 1;
  return value.substr(begin, end - begin);
}

static std::string ToLower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                 { return (char)std::tolower(c); });
  return value;
}

static std::wstring ToWide(const std::string& value)
{
  return std::wstring(value.begin(), value.end());
}

static std::string ToNarrow(const std::wstring& value)
{
  std::string result = {};
  result.reserve(value.size());
  for (wchar_t ch : value)
  {
    result.push_back(ch >= 0 && ch <= 0x7f ? (char)ch : '?');
  }
  return result;
}

static std::wstring FitWide(const std::wstring& value, int width)
{
  if (width <= 0) return {};
  if ((int)value.size() <= width) return value;
  if (width <= 3) return value.substr(0, (size_t)width);
  return value.substr(0, (size_t)(width - 3)) + L"...";
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

static nlohmann::json& EnsureRuntimeObject(nlohmann::json& root)
{
  return EnsureObject(root, "runtime");
}

static nlohmann::json& EnsureSimulationObject(nlohmann::json& root)
{
  return EnsureObject(root, "simulation");
}

static nlohmann::json& EnsureServiceObject(nlohmann::json& root)
{
  return EnsureObject(root, "service");
}

static nlohmann::json& EnsureRuntimeInterestObject(nlohmann::json& root)
{
  return EnsureObject(EnsureRuntimeObject(root), "interest");
}

static void NormalizeConfigSchema(nlohmann::json* root)
{
  if (root == nullptr || !root->is_object()) return;

  EnsureObject(*root, "wallet");
  nlohmann::json& runtimeJson = EnsureRuntimeObject(*root);

  EnsureAddress(runtimeJson);
  runtimeJson.erase(std::string("node") + "Id");
  EnsureRuntimeInterestObject(*root);
  EnsureSimulationObject(*root);
  EnsureServiceObject(*root);
}

static const wchar_t* RoleTitle(NodeCliRole role)
{
  return role == NodeCliRole::Relay ? L"Relay Node" : L"Simulator Node";
}

static const wchar_t* RoleSubtitle(NodeCliRole role)
{
  return role == NodeCliRole::Relay ? L"Discovery and packet-forwarding role" : L"Headless world simulation role";
}

static std::string FormatFloat(float value)
{
  std::ostringstream stream = {};
  stream << std::fixed << std::setprecision(3) << value;
  std::string text = stream.str();
  while (!text.empty() && text.back() == '0') text.pop_back();
  if (!text.empty() && text.back() == '.') text.pop_back();
  return text.empty() ? "0" : text;
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

struct RealmPreview
{
  bool loaded = false;
  std::string error = {};
  std::string directory = {};
  std::string name = {};
  std::string rpcUrl = {};
  std::string globalParamsAddress = {};
  std::string playerRegistryAddress = {};
  std::string chunkClaimsAddress = {};
  std::string marketplaceAddress = {};
  std::vector<std::string> jumpNodes = {};
};

static RealmPreview LoadRealmPreview(const std::string& realmDirectory)
{
  RealmPreview preview = {};
  preview.directory = realmDirectory;

  nlohmann::json realm = {};
  nlohmann::json jumpNodes = {};
  std::string error = {};
  if (!LoadJsonFile(realmDirectory + "/realm.json", &realm, &error))
  {
    preview.error = error;
    return preview;
  }

  if (!LoadJsonFile(realmDirectory + "/jump_nodes.json", &jumpNodes, &error))
  {
    preview.error = error;
    return preview;
  }

  preview.loaded = true;
  preview.name = JsonString(realm, "name", realmDirectory);
  const nlohmann::json blockchain = realm.value("blockchain", nlohmann::json::object());
  preview.rpcUrl = JsonString(blockchain, "rpcUrl", {});
  preview.globalParamsAddress = JsonString(blockchain, "globalParamsAddress", {});
  preview.playerRegistryAddress = JsonString(blockchain, "playerRegistryAddress", {});
  preview.chunkClaimsAddress = JsonString(blockchain, "chunkClaimsAddress", {});
  preview.marketplaceAddress = JsonString(blockchain, "marketplaceAddress", {});

  const nlohmann::json jumpNodeArray = jumpNodes.value("jumpNodes", nlohmann::json::array());
  if (jumpNodeArray.is_array())
  {
    for (const nlohmann::json& node : jumpNodeArray)
    {
      const std::string host = JsonString(node, "host", {});
      const int port = JsonInt(node, "port", 0);
      const std::string label = JsonString(node, "label", {});
      if (host.empty() || port <= 0) continue;
      std::string text = host + ":" + std::to_string(port);
      if (!label.empty()) text += "  (" + label + ")";
      preview.jumpNodes.push_back(text);
    }
  }

  return preview;
}

enum class NodeCliFieldKind
{
  Realm,
  String,
  Int,
  Float,
  Bool,
};

enum class NodeCliFieldId
{
  Realm,
  WalletAccountAddress,
  BindHost,
  BindPort,
  RelayReceiveTimeoutMs,
  RelayTicks,
  SimulatorJumpNodeIndex,
  SimulatorInterestChunkX,
  SimulatorInterestChunkY,
  SimulatorInterestRadius,
  SimulatorFrames,
  SimulatorFrameTime,
  SimulatorSleepMs,
  SimulatorRuntimeEnabled,
  SimulatorEmitPlaceEvent,
  SimulatorPlaceVoxelValue,
  SimulatorReceiveTimeoutMs,
};

struct NodeCliFieldSpec
{
  NodeCliFieldId id = NodeCliFieldId::Realm;
  NodeCliFieldKind kind = NodeCliFieldKind::String;
  const wchar_t* section = L"";
  const wchar_t* label = L"";
  const wchar_t* help = L"";
};

static const std::vector<NodeCliFieldSpec>& GetRelayFields()
{
  static const std::vector<NodeCliFieldSpec> fields = {
      {                NodeCliFieldId::Realm,  NodeCliFieldKind::Realm,  L"Realm",     L"Selected realm", L"Choose the runtime realm directory used for blockchain and jump-node data."},
      { NodeCliFieldId::WalletAccountAddress, NodeCliFieldKind::String, L"Wallet",    L"Account address",                        L"Wallet account used for ownership/economic actions."},
      {             NodeCliFieldId::BindHost, NodeCliFieldKind::String,  L"Relay",          L"Bind host",                                     L"Interface the relay listener binds to."},
      {             NodeCliFieldId::BindPort,    NodeCliFieldKind::Int,  L"Relay",          L"Bind port",                                        L"UDP port exposed by the relay node."},
      {NodeCliFieldId::RelayReceiveTimeoutMs,    NodeCliFieldKind::Int,  L"Relay", L"Receive timeout ms",                            L"Polling timeout used while waiting for packets."},
      {           NodeCliFieldId::RelayTicks,    NodeCliFieldKind::Int,  L"Relay",       L"Ticks budget",   L"0 runs indefinitely; positive values stop after that many service ticks."},
  };
  return fields;
}

static const std::vector<NodeCliFieldSpec>& GetSimulatorFields()
{
  static const std::vector<NodeCliFieldSpec> fields = {
      {                    NodeCliFieldId::Realm,  NodeCliFieldKind::Realm,      L"Realm",     L"Selected realm", L"Choose the runtime realm directory used for blockchain and jump-node data."},
      {     NodeCliFieldId::WalletAccountAddress, NodeCliFieldKind::String,     L"Wallet",    L"Account address",                        L"Wallet account used for ownership/economic actions."},
      {                 NodeCliFieldId::BindHost, NodeCliFieldKind::String,    L"Runtime",          L"Bind host",                         L"Interface the simulator runtime listener binds to."},
      {                 NodeCliFieldId::BindPort,    NodeCliFieldKind::Int,    L"Runtime",          L"Bind port",                                    L"UDP port exposed by the simulator node."},
      {   NodeCliFieldId::SimulatorJumpNodeIndex,    NodeCliFieldKind::Int,    L"Runtime",    L"Jump node index",             L"Which discovered jump node becomes the relay/bootstrap target."},
      {  NodeCliFieldId::SimulatorInterestChunkX,    NodeCliFieldKind::Int,    L"Runtime",   L"Interest chunk X",                      L"Center chunk X used when publishing runtime interest."},
      {  NodeCliFieldId::SimulatorInterestChunkY,    NodeCliFieldKind::Int,    L"Runtime",   L"Interest chunk Y",                      L"Center chunk Y used when publishing runtime interest."},
      {  NodeCliFieldId::SimulatorInterestRadius,    NodeCliFieldKind::Int,    L"Runtime",    L"Interest radius",           L"Chunk radius advertised to relay nodes for forwarding decisions."},
      {          NodeCliFieldId::SimulatorFrames,    NodeCliFieldKind::Int, L"Simulation",             L"Frames",   L"0 runs indefinitely; positive values stop after that many world updates."},
      {       NodeCliFieldId::SimulatorFrameTime,  NodeCliFieldKind::Float, L"Simulation",         L"Frame time",                                    L"Fixed world-update timestep in seconds."},
      {         NodeCliFieldId::SimulatorSleepMs,    NodeCliFieldKind::Int, L"Simulation",           L"Sleep ms",               L"Sleep inserted between frames to throttle the headless loop."},
      {  NodeCliFieldId::SimulatorRuntimeEnabled,   NodeCliFieldKind::Bool, L"Simulation",    L"Runtime enabled",          L"Enable ENet runtime startup, handshake, and interest publication."},
      {  NodeCliFieldId::SimulatorEmitPlaceEvent,   NodeCliFieldKind::Bool, L"Simulation",   L"Emit place event",                      L"Send a test place-world-event during runtime startup."},
      { NodeCliFieldId::SimulatorPlaceVoxelValue,    NodeCliFieldKind::Int, L"Simulation",  L"Place voxel value",                      L"Voxel value written by the optional test place event."},
      {NodeCliFieldId::SimulatorReceiveTimeoutMs,    NodeCliFieldKind::Int, L"Simulation", L"Receive timeout ms",        L"Timeout used while waiting for relay discovery and runtime packets."},
  };
  return fields;
}

static const std::vector<NodeCliFieldSpec>& GetFieldSpecs(NodeCliRole role)
{
  return role == NodeCliRole::Relay ? GetRelayFields() : GetSimulatorFields();
}

static std::string GetFieldValue(const nlohmann::json& root, NodeCliFieldId fieldId)
{
  const nlohmann::json wallet = root.value("wallet", nlohmann::json::object());
  const nlohmann::json runtimeJson = root.value("runtime", nlohmann::json::object());
  const nlohmann::json simulationJson = root.value("simulation", nlohmann::json::object());
  const nlohmann::json serviceJson = root.value("service", nlohmann::json::object());
  const nlohmann::json bindAddress = runtimeJson.value("bindAddress", nlohmann::json::object());
  const nlohmann::json interestJson = runtimeJson.value("interest", nlohmann::json::object());

  switch (fieldId)
  {
    case NodeCliFieldId::Realm:                     return JsonString(root, "realm", "realms/test");
    case NodeCliFieldId::WalletAccountAddress:      return JsonString(wallet, "accountAddress", {});
    case NodeCliFieldId::BindHost:                  return JsonString(bindAddress, "host", "127.0.0.1");
    case NodeCliFieldId::BindPort:                  return std::to_string(JsonInt(bindAddress, "port", 46010));
    case NodeCliFieldId::RelayReceiveTimeoutMs:     return std::to_string(JsonInt(runtimeJson, "receiveTimeoutMs", 1000));
    case NodeCliFieldId::RelayTicks:                return std::to_string(JsonInt(serviceJson, "ticks", 0));
    case NodeCliFieldId::SimulatorJumpNodeIndex:    return std::to_string(JsonInt(runtimeJson, "jumpNodeIndex", 0));
    case NodeCliFieldId::SimulatorInterestChunkX:   return std::to_string(JsonInt(interestJson, "chunkX", 0));
    case NodeCliFieldId::SimulatorInterestChunkY:   return std::to_string(JsonInt(interestJson, "chunkY", 0));
    case NodeCliFieldId::SimulatorInterestRadius:   return std::to_string(JsonInt(interestJson, "radius", 1));
    case NodeCliFieldId::SimulatorFrames:           return std::to_string(JsonInt(simulationJson, "frames", 0));
    case NodeCliFieldId::SimulatorFrameTime:        return FormatFloat(JsonFloat(simulationJson, "frameTime", 1.0f / 60.0f));
    case NodeCliFieldId::SimulatorSleepMs:          return std::to_string(JsonInt(simulationJson, "sleepMs", 16));
    case NodeCliFieldId::SimulatorRuntimeEnabled:   return JsonBool(runtimeJson, "enabled", false) ? "true" : "false";
    case NodeCliFieldId::SimulatorEmitPlaceEvent:   return JsonBool(simulationJson, "emitPlaceEvent", false) ? "true" : "false";
    case NodeCliFieldId::SimulatorPlaceVoxelValue:  return std::to_string(JsonInt(simulationJson, "placeVoxelValue", 255));
    case NodeCliFieldId::SimulatorReceiveTimeoutMs: return std::to_string(JsonInt(runtimeJson, "receiveTimeoutMs", 1000));
  }

  return {};
}

static bool ParseInt64(const std::string& value, int64_t* parsed)
{
  if (parsed == nullptr) return false;
  char* end = nullptr;
  const long long number = std::strtoll(value.c_str(), &end, 10);
  if (end == value.c_str() || (end != nullptr && *end != '\0')) return false;
  *parsed = (int64_t)number;
  return true;
}

static bool ParseFloatValue(const std::string& value, float* parsed)
{
  if (parsed == nullptr) return false;
  char* end = nullptr;
  const float number = std::strtof(value.c_str(), &end);
  if (end == value.c_str() || (end != nullptr && *end != '\0')) return false;
  *parsed = number;
  return true;
}

static bool SetFieldValue(
    nlohmann::json& root,
    NodeCliFieldId fieldId,
    const std::string& rawValue,
    std::string* errorMessage)
{
  const std::string value = Trim(rawValue);
  nlohmann::json& wallet = EnsureObject(root, "wallet");
  nlohmann::json& runtimeJson = EnsureRuntimeObject(root);
  nlohmann::json& bindAddress = EnsureAddress(runtimeJson);
  nlohmann::json& interestJson = EnsureRuntimeInterestObject(root);
  nlohmann::json& simulationJson = EnsureSimulationObject(root);
  nlohmann::json& serviceJson = EnsureServiceObject(root);

  auto fail = [&](const std::string& error)
  {
    if (errorMessage != nullptr) *errorMessage = error;
    return false;
  };

  switch (fieldId)
  {
    case NodeCliFieldId::Realm:
      if (value.empty()) return fail("realm directory cannot be empty");
      root["realm"] = value;
      return true;

    case NodeCliFieldId::WalletAccountAddress:
      wallet["accountAddress"] = value;
      return true;

    case NodeCliFieldId::BindHost:
      if (value.empty()) return fail("bind host cannot be empty");
      bindAddress["host"] = value;
      return true;

    case NodeCliFieldId::BindPort:
    {
      int64_t parsed = 0;
      if (!ParseInt64(value, &parsed)) return fail("bind port must be an integer");
      if (parsed < 1 || parsed > 65535) return fail("bind port must be between 1 and 65535");
      bindAddress["port"] = (int)parsed;
      return true;
    }

    case NodeCliFieldId::RelayReceiveTimeoutMs:
    case NodeCliFieldId::SimulatorReceiveTimeoutMs:
    {
      int64_t parsed = 0;
      if (!ParseInt64(value, &parsed)) return fail("timeout must be an integer");
      if (parsed < 0 || parsed > 600000) return fail("timeout must be between 0 and 600000");
      runtimeJson["receiveTimeoutMs"] = (int)parsed;
      return true;
    }

    case NodeCliFieldId::RelayTicks:
    {
      int64_t parsed = 0;
      if (!ParseInt64(value, &parsed)) return fail("ticks budget must be an integer");
      if (parsed < 0 || parsed > 1000000000LL) return fail("ticks budget must be 0 or positive");
      serviceJson["ticks"] = (int)parsed;
      return true;
    }

    case NodeCliFieldId::SimulatorJumpNodeIndex:
    {
      int64_t parsed = 0;
      if (!ParseInt64(value, &parsed)) return fail("jump node index must be an integer");
      if (parsed < 0 || parsed > 1024) return fail("jump node index must be 0 or positive");
      runtimeJson["jumpNodeIndex"] = (int)parsed;
      return true;
    }

    case NodeCliFieldId::SimulatorInterestChunkX:
    case NodeCliFieldId::SimulatorInterestChunkY:
    {
      int64_t parsed = 0;
      if (!ParseInt64(value, &parsed)) return fail("interest chunk must be an integer");
      if (parsed < -30000 || parsed > 30000) return fail("interest chunk must stay inside [-30000, 30000]");
      interestJson[fieldId == NodeCliFieldId::SimulatorInterestChunkX ? "chunkX" : "chunkY"] = (int)parsed;
      return true;
    }

    case NodeCliFieldId::SimulatorInterestRadius:
    {
      int64_t parsed = 0;
      if (!ParseInt64(value, &parsed)) return fail("interest radius must be an integer");
      if (parsed < 0 || parsed > 1024) return fail("interest radius must be between 0 and 1024");
      interestJson["radius"] = (int)parsed;
      return true;
    }

    case NodeCliFieldId::SimulatorFrames:
    {
      int64_t parsed = 0;
      if (!ParseInt64(value, &parsed)) return fail("frames must be an integer");
      if (parsed < 0 || parsed > 1000000000LL) return fail("frames must be 0 or positive");
      simulationJson["frames"] = (int)parsed;
      return true;
    }

    case NodeCliFieldId::SimulatorFrameTime:
    {
      float parsed = 0.0f;
      if (!ParseFloatValue(value, &parsed)) return fail("frame time must be a number");
      if (parsed <= 0.0f || parsed > 10.0f) return fail("frame time must be between 0 and 10 seconds");
      simulationJson["frameTime"] = parsed;
      return true;
    }

    case NodeCliFieldId::SimulatorSleepMs:
    {
      int64_t parsed = 0;
      if (!ParseInt64(value, &parsed)) return fail("sleep ms must be an integer");
      if (parsed < 0 || parsed > 600000) return fail("sleep ms must be between 0 and 600000");
      simulationJson["sleepMs"] = (int)parsed;
      return true;
    }

    case NodeCliFieldId::SimulatorRuntimeEnabled:
    case NodeCliFieldId::SimulatorEmitPlaceEvent:
    {
      const std::string lowered = ToLower(value);
      if (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "y")
      {
        (fieldId == NodeCliFieldId::SimulatorRuntimeEnabled ? runtimeJson : simulationJson)
            [fieldId == NodeCliFieldId::SimulatorRuntimeEnabled ? "enabled" : "emitPlaceEvent"] = true;
        return true;
      }
      if (lowered == "false" || lowered == "0" || lowered == "no" || lowered == "n")
      {
        (fieldId == NodeCliFieldId::SimulatorRuntimeEnabled ? runtimeJson : simulationJson)
            [fieldId == NodeCliFieldId::SimulatorRuntimeEnabled ? "enabled" : "emitPlaceEvent"] = false;
        return true;
      }
      return fail("boolean fields accept true/false, yes/no, or 1/0");
    }

    case NodeCliFieldId::SimulatorPlaceVoxelValue:
    {
      int64_t parsed = 0;
      if (!ParseInt64(value, &parsed)) return fail("place voxel value must be an integer");
      if (parsed < 0 || parsed > 255) return fail("place voxel value must be between 0 and 255");
      simulationJson["placeVoxelValue"] = (int)parsed;
      return true;
    }
  }

  return fail("unknown field");
}

static bool ToggleBoolField(nlohmann::json& root, NodeCliFieldId fieldId)
{
  nlohmann::json& runtimeJson = EnsureRuntimeObject(root);
  nlohmann::json& simulationJson = EnsureSimulationObject(root);
  switch (fieldId)
  {
    case NodeCliFieldId::SimulatorRuntimeEnabled:
      runtimeJson["enabled"] = !JsonBool(runtimeJson, "enabled", false);
      return true;
    case NodeCliFieldId::SimulatorEmitPlaceEvent:
      simulationJson["emitPlaceEvent"] = !JsonBool(simulationJson, "emitPlaceEvent", false);
      return true;
    default: return false;
  }
}

static int GetVisualRowForField(const std::vector<NodeCliFieldSpec>& fields, size_t selectedIndex)
{
  int row = 0;
  const wchar_t* lastSection = nullptr;
  for (size_t i = 0; i < fields.size(); ++i)
  {
    if (lastSection == nullptr || fields[i].section != lastSection)
    {
      row += 1;
      lastSection = fields[i].section;
    }
    if (i == selectedIndex) return row;
    row += 1;
  }
  return row;
}

enum class StatusTone
{
  Info,
  Success,
  Warning,
  Error,
};

struct EditorState
{
  bool active = false;
  NodeCliFieldSpec field = {};
  std::string input = {};
  std::string error = {};
};

struct RealmPickerState
{
  bool active = false;
  std::vector<std::string> realms = {};
  size_t selectedIndex = 0;
};

struct NodeCliUiState
{
  size_t selectedFieldIndex = 0;
  bool dirty = false;
  bool exitArmed = false;
  bool reloadArmed = false;
  std::string status = "Arrows navigate. Enter edits. S saves. L saves and launches. N launches without saving.";
  StatusTone statusTone = StatusTone::Info;
  EditorState editor = {};
  RealmPickerState realmPicker = {};
  RealmPreview realmPreview = {};
  std::string previewRealm = {};
};

static void SetStatus(NodeCliUiState* state, StatusTone tone, const std::string& status)
{
  if (state == nullptr) return;
  state->statusTone = tone;
  state->status = status;
}

static void RefreshRealmPreview(const nlohmann::json& root, NodeCliUiState* state)
{
  if (state == nullptr) return;
  const std::string realm = JsonString(root, "realm", "realms/test");
  if (state->previewRealm == realm && (state->realmPreview.loaded || !state->realmPreview.error.empty())) return;
  state->previewRealm = realm;
  state->realmPreview = LoadRealmPreview(realm);
}

static tcolor_t ToneColor(StatusTone tone)
{
  switch (tone)
  {
    case StatusTone::Info:    return COLOR_INFO;
    case StatusTone::Success: return COLOR_SUCCESS;
    case StatusTone::Warning: return COLOR_WARNING;
    case StatusTone::Error:   return COLOR_ERROR;
  }
  return COLOR_INFO;
}

static void FillRect(int x, int y, int width, int height, tcolor_t bg)
{
  for (int row = 0; row < height; ++row)
  {
    for (int column = 0; column < width; ++column)
    {
      tput(x + column, y + row, L' ', COLOR_TEXT, bg);
    }
  }
}

static void DrawTextLine(int x, int y, int width, const std::wstring& text, tcolor_t fg, tcolor_t bg)
{
  if (width <= 0) return;
  for (int i = 0; i < width; ++i)
  {
    tput(x + i, y, L' ', fg, bg);
  }
  twrite(x, y, FitWide(text, width).c_str(), fg, bg);
}

static void DrawFrame(int x, int y, int width, int height, const std::wstring& title, tcolor_t bg, tcolor_t border)
{
  if (width < 2 || height < 2) return;
  FillRect(x, y, width, height, bg);

  for (int column = 0; column < width; ++column)
  {
    tput(x + column, y, column == 0 || column == width - 1 ? L'+' : L'-', border, bg);
    tput(x + column, y + height - 1, column == 0 || column == width - 1 ? L'+' : L'-', border, bg);
  }

  for (int row = 1; row < height - 1; ++row)
  {
    tput(x, y + row, L'|', border, bg);
    tput(x + width - 1, y + row, L'|', border, bg);
  }

  if (!title.empty())
  {
    DrawTextLine(x + 2, y, std::max(0, width - 4), L" " + title + L" ", border, bg);
  }
}

static void DrawWrappedLines(int x, int y, int width, int maxLines, const std::vector<std::string>& lines, tcolor_t fg, tcolor_t bg)
{
  int row = 0;
  for (const std::string& line : lines)
  {
    if (row >= maxLines) break;
    DrawTextLine(x, y + row, width, ToWide(line), fg, bg);
    row += 1;
  }
  for (; row < maxLines; ++row)
  {
    DrawTextLine(x, y + row, width, L"", fg, bg);
  }
}

static std::vector<std::string> BuildSummaryLines(const nlohmann::json& root, NodeCliRole role, const NodeCliUiState& state)
{
  std::vector<std::string> lines = {};
  lines.push_back(std::string("Role: ") + (role == NodeCliRole::Relay ? "relay" : "simulator"));
  lines.push_back(std::string("Config realm: ") + JsonString(root, "realm", "realms/test"));
  lines.push_back(std::string("Wallet account: ") + JsonString(root.value("wallet", nlohmann::json::object()), "accountAddress", {}));

  const nlohmann::json runtimeJson = root.value("runtime", nlohmann::json::object());
  const nlohmann::json simulationJson = root.value("simulation", nlohmann::json::object());
  const nlohmann::json serviceJson = root.value("service", nlohmann::json::object());
  const nlohmann::json bindAddress = runtimeJson.value("bindAddress", nlohmann::json::object());
  const nlohmann::json interestJson = runtimeJson.value("interest", nlohmann::json::object());
  lines.push_back(
      std::string("Bind: ") + JsonString(bindAddress, "host", "127.0.0.1") + ":" +
      std::to_string(JsonInt(bindAddress, "port", 46010)));

  if (role == NodeCliRole::Relay)
  {
    lines.push_back(std::string("Receive timeout ms: ") + std::to_string(JsonInt(runtimeJson, "receiveTimeoutMs", 1000)));
    lines.push_back(std::string("Ticks budget: ") + std::to_string(JsonInt(serviceJson, "ticks", 0)));
  }
  else
  {
    lines.push_back(std::string("Jump node index: ") + std::to_string(JsonInt(runtimeJson, "jumpNodeIndex", 0)));
    lines.push_back(std::string("Runtime enabled: ") + (JsonBool(runtimeJson, "enabled", false) ? "true" : "false"));
    lines.push_back(std::string("Emit place event: ") + (JsonBool(simulationJson, "emitPlaceEvent", false) ? "true" : "false"));
    lines.push_back(std::string("Frames: ") + std::to_string(JsonInt(simulationJson, "frames", 0)));
    lines.push_back(std::string("Frame time: ") + FormatFloat(JsonFloat(simulationJson, "frameTime", 1.0f / 60.0f)));
    lines.push_back(std::string("Sleep ms: ") + std::to_string(JsonInt(simulationJson, "sleepMs", 16)));

    const int jumpNodeIndex = JsonInt(runtimeJson, "jumpNodeIndex", 0);
    if (state.realmPreview.loaded && jumpNodeIndex >= 0 && (size_t)jumpNodeIndex < state.realmPreview.jumpNodes.size())
    {
      lines.push_back(std::string("Selected jump node: ") + state.realmPreview.jumpNodes[(size_t)jumpNodeIndex]);
    }
    lines.push_back(std::string("Interest chunk: ") + std::to_string(JsonInt(interestJson, "chunkX", 0)) + "," + std::to_string(JsonInt(interestJson, "chunkY", 0)));
    lines.push_back(std::string("Interest radius: ") + std::to_string(JsonInt(interestJson, "radius", 1)));
  }

  return lines;
}

static void RenderMainUi(const NodeCliOptions& options, const nlohmann::json& root, const NodeCliUiState& state)
{
  tclear();
  FillRect(0, 0, twidth(), theight(), COLOR_BG);

  const int width = twidth();
  const int height = theight();
  const int footerHeight = 3;
  const int bodyY = 2;
  const int bodyHeight = height - bodyY - footerHeight;
  const int leftWidth = std::max(42, width * 11 / 20);
  const int rightX = leftWidth + 1;
  const int rightWidth = width - rightX;

  DrawTextLine(0, 0, width, L" OpenRealm node/cli ", COLOR_TEXT, COLOR_HEADER);
  DrawTextLine(22, 0, std::max(0, width - 22), std::wstring(RoleTitle(options.role)) + L"  |  " + RoleSubtitle(options.role), COLOR_HEADER_ACCENT, COLOR_HEADER);

  std::wstring headerStatus = state.dirty ? L"DIRTY" : L"CLEAN";
  DrawTextLine(std::max(0, width - 14), 0, 14, headerStatus, state.dirty ? COLOR_WARNING : COLOR_SUCCESS, COLOR_HEADER);

  DrawFrame(0, bodyY, leftWidth, bodyHeight, L"Config", COLOR_PANEL, COLOR_BORDER);
  DrawFrame(rightX, bodyY, rightWidth, bodyHeight, L"Context", COLOR_PANEL_ALT, COLOR_BORDER);

  const std::vector<NodeCliFieldSpec>& fields = GetFieldSpecs(options.role);
  const int contentX = 2;
  const int contentY = bodyY + 1;
  const int contentWidth = leftWidth - 4;
  const int contentHeight = bodyHeight - 2;
  const int selectedVisualRow = GetVisualRowForField(fields, state.selectedFieldIndex);
  int scrollRow = std::max(0, selectedVisualRow - contentHeight + 3);

  int visualRow = 0;
  const wchar_t* lastSection = nullptr;
  for (size_t i = 0; i < fields.size(); ++i)
  {
    const NodeCliFieldSpec& field = fields[i];
    if (lastSection == nullptr || field.section != lastSection)
    {
      if (visualRow >= scrollRow && visualRow < scrollRow + contentHeight)
      {
        DrawTextLine(contentX, contentY + visualRow - scrollRow, contentWidth, field.section, COLOR_SECTION, COLOR_PANEL);
      }
      visualRow += 1;
      lastSection = field.section;
    }

    if (visualRow >= scrollRow && visualRow < scrollRow + contentHeight)
    {
      const bool selected = i == state.selectedFieldIndex;
      const std::string valueText = GetFieldValue(root, field.id);
      const tcolor_t bg = selected ? COLOR_SELECTED_BG : COLOR_PANEL;
      const tcolor_t border = selected ? COLOR_SELECTED_BORDER : bg;
      const tcolor_t valueColor = field.kind == NodeCliFieldKind::Bool
                                      ? (valueText == "true" ? COLOR_BOOL_TRUE : COLOR_BOOL_FALSE)
                                      : COLOR_TEXT;

      DrawTextLine(contentX, contentY + visualRow - scrollRow, contentWidth, L"", COLOR_TEXT, bg);
      DrawTextLine(contentX, contentY + visualRow - scrollRow, FIELD_LABEL_WIDTH, field.label, COLOR_MUTED, bg);
      DrawTextLine(contentX + FIELD_LABEL_WIDTH, contentY + visualRow - scrollRow, std::max(0, contentWidth - FIELD_LABEL_WIDTH - 3), ToWide(valueText), valueColor, bg);
      tput(contentX - 1, contentY + visualRow - scrollRow, selected ? L'>' : L' ', border, COLOR_PANEL);
    }
    visualRow += 1;
  }

  const NodeCliFieldSpec& selectedField = fields[state.selectedFieldIndex];
  const int contextX = rightX + 2;
  const int contextY = bodyY + 1;
  const int contextWidth = rightWidth - 4;
  int row = 0;

  DrawTextLine(contextX, contextY + row++, contextWidth, L"Selected field", COLOR_SECTION, COLOR_PANEL_ALT);
  DrawTextLine(contextX, contextY + row++, contextWidth, selectedField.label, COLOR_TEXT, COLOR_PANEL_ALT);
  DrawTextLine(contextX, contextY + row++, contextWidth, selectedField.help, COLOR_MUTED, COLOR_PANEL_ALT);
  row += 1;

  DrawTextLine(contextX, contextY + row++, contextWidth, L"Realm preview", COLOR_SECTION, COLOR_PANEL_ALT);
  if (!state.realmPreview.loaded)
  {
    DrawTextLine(contextX, contextY + row++, contextWidth, ToWide(state.realmPreview.error.empty() ? "failed to load realm preview" : state.realmPreview.error), COLOR_ERROR, COLOR_PANEL_ALT);
  }
  else
  {
    DrawTextLine(contextX, contextY + row++, contextWidth, ToWide("Directory: " + state.realmPreview.directory), COLOR_TEXT, COLOR_PANEL_ALT);
    DrawTextLine(contextX, contextY + row++, contextWidth, ToWide("Name: " + state.realmPreview.name), COLOR_TEXT, COLOR_PANEL_ALT);
    DrawTextLine(contextX, contextY + row++, contextWidth, ToWide("RPC: " + state.realmPreview.rpcUrl), COLOR_TEXT, COLOR_PANEL_ALT);
    DrawTextLine(contextX, contextY + row++, contextWidth, ToWide("Jump nodes: " + std::to_string(state.realmPreview.jumpNodes.size())), COLOR_TEXT, COLOR_PANEL_ALT);
    for (const std::string& node : state.realmPreview.jumpNodes)
    {
      if (row >= bodyHeight - 8) break;
      DrawTextLine(contextX, contextY + row++, contextWidth, ToWide("- " + node), COLOR_MUTED, COLOR_PANEL_ALT);
    }
  }

  row += 1;
  DrawTextLine(contextX, contextY + row++, contextWidth, L"Launch summary", COLOR_SECTION, COLOR_PANEL_ALT);
  const std::vector<std::string> summaryLines = BuildSummaryLines(root, options.role, state);
  for (const std::string& line : summaryLines)
  {
    if (row >= bodyHeight - 2) break;
    DrawTextLine(contextX, contextY + row++, contextWidth, ToWide(line), COLOR_TEXT, COLOR_PANEL_ALT);
  }

  DrawTextLine(0, height - 3, width, ToWide(state.status), ToneColor(state.statusTone), COLOR_BG);
  DrawTextLine(0, height - 2, width, L" Enter edit/toggle  |  S save  |  L save+launch  |  N launch without saving  |  R reload  |  Q exit ", COLOR_MUTED, COLOR_BG);
  DrawTextLine(0, height - 1, width, L" Realm picker on selected realm  |  Home/End/Page keys jump  |  Resize is handled live ", COLOR_MUTED, COLOR_BG);
}

static void RenderEditorOverlay(const NodeCliUiState& state)
{
  if (!state.editor.active) return;

  const int width = std::max(48, twidth() * 3 / 5);
  const int height = 8;
  const int x = (twidth() - width) / 2;
  const int y = (theight() - height) / 2;

  DrawFrame(x, y, width, height, L"Edit value", COLOR_OVERLAY, COLOR_SELECTED_BORDER);
  DrawTextLine(x + 2, y + 1, width - 4, state.editor.field.label, COLOR_SECTION, COLOR_OVERLAY);
  DrawTextLine(x + 2, y + 2, width - 4, state.editor.field.help, COLOR_MUTED, COLOR_OVERLAY);
  DrawTextLine(x + 2, y + 4, width - 4, ToWide(state.editor.input), COLOR_TEXT, COLOR_SELECTED_BG);
  DrawTextLine(x + 2, y + 5, width - 4, ToWide(state.editor.error.empty() ? "Enter commits, Escape cancels, Backspace deletes." : state.editor.error), state.editor.error.empty() ? COLOR_MUTED : COLOR_ERROR, COLOR_OVERLAY);
}

static void RenderRealmPickerOverlay(const NodeCliUiState& state)
{
  if (!state.realmPicker.active) return;

  const int width = std::max(44, twidth() / 2);
  const int height = std::min(std::max(8, (int)state.realmPicker.realms.size() + 5), theight() - 4);
  const int x = (twidth() - width) / 2;
  const int y = (theight() - height) / 2;

  DrawFrame(x, y, width, height, L"Pick realm", COLOR_OVERLAY, COLOR_SELECTED_BORDER);
  DrawTextLine(x + 2, y + 1, width - 4, L"Enter selects. Escape cancels.", COLOR_MUTED, COLOR_OVERLAY);

  const int listHeight = height - 3;
  int scroll = 0;
  if ((int)state.realmPicker.selectedIndex >= listHeight)
  {
    scroll = (int)state.realmPicker.selectedIndex - listHeight + 1;
  }

  for (int row = 0; row < listHeight; ++row)
  {
    const int realmIndex = scroll + row;
    if (realmIndex >= (int)state.realmPicker.realms.size()) break;
    const bool selected = (size_t)realmIndex == state.realmPicker.selectedIndex;
    DrawTextLine(
        x + 2,
        y + 2 + row,
        width - 4,
        ToWide(state.realmPicker.realms[(size_t)realmIndex]),
        selected ? COLOR_TEXT : COLOR_MUTED,
        selected ? COLOR_SELECTED_BG : COLOR_OVERLAY);
  }
}

enum class NodeCliAction
{
  None,
  Exit,
  Launch,
};

static void OpenEditor(const nlohmann::json& root, const NodeCliFieldSpec& field, NodeCliUiState* state)
{
  if (state == nullptr) return;
  state->editor.active = true;
  state->editor.field = field;
  state->editor.input = GetFieldValue(root, field.id);
  state->editor.error.clear();
}

static void OpenRealmPicker(const nlohmann::json& root, NodeCliUiState* state)
{
  if (state == nullptr) return;
  state->realmPicker.active = true;
  state->realmPicker.realms = DiscoverRealmDirectories();
  state->realmPicker.selectedIndex = 0;

  const std::string currentRealm = JsonString(root, "realm", "realms/test");
  for (size_t i = 0; i < state->realmPicker.realms.size(); ++i)
  {
    if (state->realmPicker.realms[i] == currentRealm)
    {
      state->realmPicker.selectedIndex = i;
      break;
    }
  }
}

static NodeCliAction SaveIfNeeded(const NodeCliOptions& options, const nlohmann::json& root, NodeCliUiState* state, bool launchAfterSave)
{
  if (state == nullptr) return NodeCliAction::None;
  if (!state->dirty)
  {
    SetStatus(state, StatusTone::Info, launchAfterSave ? "Launching with current config." : "Config already clean.");
    return launchAfterSave ? NodeCliAction::Launch : NodeCliAction::None;
  }

  nlohmann::json normalizedRoot = root;
  NormalizeConfigSchema(&normalizedRoot);

  std::string error = {};
  if (!SaveJsonFile(options.configPath, normalizedRoot, &error))
  {
    SetStatus(state, StatusTone::Error, "Failed to save config: " + error);
    return NodeCliAction::None;
  }

  state->dirty = false;
  state->exitArmed = false;
  state->reloadArmed = false;
  SetStatus(state, StatusTone::Success, launchAfterSave ? "Saved config and launching." : "Saved config.json successfully.");
  return launchAfterSave ? NodeCliAction::Launch : NodeCliAction::None;
}

static NodeCliAction HandleEditorKey(
    int key,
    nlohmann::json* root,
    const NodeCliOptions& options,
    NodeCliUiState* state)
{
  (void)options;
  if (root == nullptr || state == nullptr) return NodeCliAction::None;

  if (key == TKEY_ESCAPE)
  {
    state->editor.active = false;
    state->editor.error.clear();
    SetStatus(state, StatusTone::Info, "Edit cancelled.");
    return NodeCliAction::None;
  }

  if (key == TKEY_BACKSPACE)
  {
    if (!state->editor.input.empty()) state->editor.input.pop_back();
    return NodeCliAction::None;
  }

  if (key == TKEY_ENTER)
  {
    std::string error = {};
    if (!SetFieldValue(*root, state->editor.field.id, state->editor.input, &error))
    {
      state->editor.error = error;
      return NodeCliAction::None;
    }

    state->editor.active = false;
    state->dirty = true;
    state->exitArmed = false;
    state->reloadArmed = false;
    SetStatus(state, StatusTone::Success, std::string("Updated ") + ToNarrow(state->editor.field.label) + ".");
    state->previewRealm.clear();
    return NodeCliAction::None;
  }

  if (key >= 32 && key <= 126)
  {
    state->editor.input.push_back((char)key);
    state->editor.error.clear();
  }

  return NodeCliAction::None;
}

static NodeCliAction HandleRealmPickerKey(int key, nlohmann::json* root, NodeCliUiState* state)
{
  if (root == nullptr || state == nullptr) return NodeCliAction::None;

  if (key == TKEY_ESCAPE)
  {
    state->realmPicker.active = false;
    SetStatus(state, StatusTone::Info, "Realm picker cancelled.");
    return NodeCliAction::None;
  }

  if (state->realmPicker.realms.empty())
  {
    state->realmPicker.active = false;
    SetStatus(state, StatusTone::Warning, "No realm directories were found under realms/.");
    return NodeCliAction::None;
  }

  if (key == TKEY_UP && state->realmPicker.selectedIndex > 0)
  {
    state->realmPicker.selectedIndex -= 1;
    return NodeCliAction::None;
  }

  if (key == TKEY_DOWN && state->realmPicker.selectedIndex + 1 < state->realmPicker.realms.size())
  {
    state->realmPicker.selectedIndex += 1;
    return NodeCliAction::None;
  }

  if (key == TKEY_HOME)
  {
    state->realmPicker.selectedIndex = 0;
    return NodeCliAction::None;
  }

  if (key == TKEY_END)
  {
    state->realmPicker.selectedIndex = state->realmPicker.realms.size() - 1;
    return NodeCliAction::None;
  }

  if (key == TKEY_ENTER)
  {
    root->operator[]("realm") = state->realmPicker.realms[state->realmPicker.selectedIndex];
    state->realmPicker.active = false;
    state->dirty = true;
    state->previewRealm.clear();
    SetStatus(state, StatusTone::Success, "Selected realm: " + state->realmPicker.realms[state->realmPicker.selectedIndex]);
  }

  return NodeCliAction::None;
}

static NodeCliAction HandleMainKey(
    int key,
    nlohmann::json* root,
    const NodeCliOptions& options,
    NodeCliUiState* state)
{
  if (root == nullptr || state == nullptr) return NodeCliAction::None;

  const std::vector<NodeCliFieldSpec>& fields = GetFieldSpecs(options.role);
  if (fields.empty()) return NodeCliAction::None;

  const size_t lastFieldIndex = fields.size() - 1;
  if (key != 'q' && key != 'Q' && key != 'r' && key != 'R')
  {
    state->exitArmed = false;
    state->reloadArmed = false;
  }

  switch (key)
  {
    case TKEY_UP:
      if (state->selectedFieldIndex > 0) state->selectedFieldIndex -= 1;
      return NodeCliAction::None;

    case TKEY_DOWN:
      if (state->selectedFieldIndex < lastFieldIndex) state->selectedFieldIndex += 1;
      return NodeCliAction::None;

    case TKEY_HOME:
      state->selectedFieldIndex = 0;
      return NodeCliAction::None;

    case TKEY_END:
      state->selectedFieldIndex = lastFieldIndex;
      return NodeCliAction::None;

    case TKEY_PAGEUP:
      state->selectedFieldIndex = state->selectedFieldIndex > 5 ? state->selectedFieldIndex - 5 : 0;
      return NodeCliAction::None;

    case TKEY_PAGEDOWN:
      state->selectedFieldIndex = std::min(lastFieldIndex, state->selectedFieldIndex + 5);
      return NodeCliAction::None;

    case TKEY_ENTER:
    case 'e':
    case 'E':
    {
      const NodeCliFieldSpec& field = fields[state->selectedFieldIndex];
      if (field.kind == NodeCliFieldKind::Bool)
      {
        if (ToggleBoolField(*root, field.id))
        {
          state->dirty = true;
          state->previewRealm.clear();
          SetStatus(state, StatusTone::Success, std::string("Toggled ") + ToNarrow(field.label) + ".");
        }
        return NodeCliAction::None;
      }
      if (field.kind == NodeCliFieldKind::Realm)
      {
        OpenRealmPicker(*root, state);
        if (state->realmPicker.realms.empty())
        {
          state->realmPicker.active = false;
          SetStatus(state, StatusTone::Warning, "No realm directories were found under realms/.");
        }
        return NodeCliAction::None;
      }
      OpenEditor(*root, field, state);
      return NodeCliAction::None;
    }

    case ' ':
      if (fields[state->selectedFieldIndex].kind == NodeCliFieldKind::Bool)
      {
        if (ToggleBoolField(*root, fields[state->selectedFieldIndex].id))
        {
          state->dirty = true;
          SetStatus(state, StatusTone::Success, "Boolean field toggled.");
        }
      }
      return NodeCliAction::None;

    case 's':
    case 'S': return SaveIfNeeded(options, *root, state, false);

    case 'l':
    case 'L': return SaveIfNeeded(options, *root, state, true);

    case 'n':
    case 'N':
      SetStatus(state, StatusTone::Warning, state->dirty ? "Launching without saving dirty changes." : "Launching without saving.");
      return NodeCliAction::Launch;

    case 'r':
    case 'R':
    {
      if (state->dirty && !state->reloadArmed)
      {
        state->reloadArmed = true;
        SetStatus(state, StatusTone::Warning, "Unsaved changes will be lost. Press R again to reload from disk.");
        return NodeCliAction::None;
      }

      nlohmann::json reloaded = {};
      std::string error = {};
      if (!LoadJsonFile(options.configPath, &reloaded, &error))
      {
        SetStatus(state, StatusTone::Error, "Failed to reload config: " + error);
        return NodeCliAction::None;
      }

      NormalizeConfigSchema(&reloaded);
      *root = std::move(reloaded);
      state->dirty = false;
      state->reloadArmed = false;
      state->exitArmed = false;
      state->previewRealm.clear();
      SetStatus(state, StatusTone::Success, "Reloaded config.json from disk.");
      return NodeCliAction::None;
    }

    case 'q':
    case 'Q':
    case TKEY_ESCAPE:
      if (state->dirty && !state->exitArmed)
      {
        state->exitArmed = true;
        SetStatus(state, StatusTone::Warning, "Unsaved changes will be lost. Press Q again to exit.");
        return NodeCliAction::None;
      }
      return NodeCliAction::Exit;
  }

  return NodeCliAction::None;
}

bool RunNodeCli(const NodeCliOptions& options)
{
  nlohmann::json root = {};
  std::string error = {};
  if (!LoadJsonFile(options.configPath, &root, &error))
  {
    std::printf("OpenRealm config CLI failed: %s\n", error.c_str());
    return false;
  }

  NormalizeConfigSchema(&root);

  if (!tinit())
  {
    std::printf("OpenRealm config CLI failed: unable to initialize term.h terminal UI\n");
    return false;
  }

  NodeCliUiState state = {};
  RefreshRealmPreview(root, &state);

  bool launch = false;
  bool running = true;
  while (running)
  {
    RefreshRealmPreview(root, &state);
    RenderMainUi(options, root, state);
    RenderEditorOverlay(state);
    RenderRealmPickerOverlay(state);
    trender();

    const int key = twait();
    if (key == TKEY_RESIZE) continue;

    if (twidth() < MIN_TERM_WIDTH || theight() < MIN_TERM_HEIGHT)
    {
      SetStatus(&state, StatusTone::Warning, "Terminal too small. Resize to at least 90x26 for the full editor.");
    }

    NodeCliAction action = NodeCliAction::None;
    if (state.editor.active)
    {
      action = HandleEditorKey(key, &root, options, &state);
    }
    else if (state.realmPicker.active)
    {
      action = HandleRealmPickerKey(key, &root, &state);
    }
    else
    {
      action = HandleMainKey(key, &root, options, &state);
    }

    if (action == NodeCliAction::Launch)
    {
      launch = true;
      running = false;
    }
    else if (action == NodeCliAction::Exit)
    {
      launch = false;
      running = false;
    }
  }

  tquit();
  return launch;
}

static bool gRuntimeViewActive = false;
static NodeRuntimeViewOptions gRuntimeViewOptions = {};

static std::wstring RuntimeViewTitle()
{
  if (!gRuntimeViewOptions.title.empty()) return ToWide(gRuntimeViewOptions.title);
  return gRuntimeViewOptions.role == NodeCliRole::Relay ? L"OpenRealm Relay" : L"OpenRealm Simulator";
}

static std::wstring RuntimeViewSubtitle()
{
  if (!gRuntimeViewOptions.subtitle.empty()) return ToWide(gRuntimeViewOptions.subtitle);
  return L"Live node session";
}

bool BeginNodeRuntimeView(const NodeRuntimeViewOptions& options)
{
  if (!tinit()) return false;
  gRuntimeViewActive = true;
  gRuntimeViewOptions = options;
  return true;
}

bool PumpNodeRuntimeView(const std::string& status, const std::vector<NodeRuntimeViewLine>& lines)
{
  if (!gRuntimeViewActive) return false;

  tclear();
  FillRect(0, 0, twidth(), theight(), COLOR_BG);

  const int width = twidth();
  const int height = theight();
  DrawTextLine(0, 0, width, L" OpenRealm runtime ", COLOR_TEXT, COLOR_HEADER);
  DrawTextLine(22, 0, std::max(0, width - 22), RuntimeViewTitle() + L"  |  " + RuntimeViewSubtitle(), COLOR_HEADER_ACCENT, COLOR_HEADER);

  DrawFrame(0, 2, width, std::max(6, height - 5), L"Live values", COLOR_PANEL, COLOR_BORDER);

  int row = 3;
  const int valueWidth = std::max(20, width - FIELD_LABEL_WIDTH - 8);
  for (const NodeRuntimeViewLine& line : lines)
  {
    if (row >= height - 3) break;
    DrawTextLine(2, row, FIELD_LABEL_WIDTH, ToWide(line.label), COLOR_MUTED, COLOR_PANEL);
    DrawTextLine(2 + FIELD_LABEL_WIDTH, row, valueWidth, ToWide(line.value), COLOR_TEXT, COLOR_PANEL);
    row += 1;
  }

  DrawTextLine(0, height - 2, width, ToWide(status), COLOR_INFO, COLOR_BG);
  DrawTextLine(0, height - 1, width, L" Q / Esc quits cleanly  |  Values stay live while the node runs ", COLOR_MUTED, COLOR_BG);
  trender();

  for (;;)
  {
    const int key = tpoll();
    if (key == TKEY_NONE || key == TKEY_RESIZE) break;
    if (key == 'q' || key == 'Q' || key == TKEY_ESCAPE) return true;
  }

  return false;
}

void EndNodeRuntimeView()
{
  if (!gRuntimeViewActive) return;
  tquit();
  gRuntimeViewActive = false;
  gRuntimeViewOptions = {};
}
