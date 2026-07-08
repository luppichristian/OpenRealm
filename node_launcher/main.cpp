#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#if defined(_WIN32)
#  define NOMINMAX
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <signal.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

struct LaunchOptions
{
  std::filesystem::path repoRoot = {};
  std::filesystem::path baseConfigPath = "config.json";
  std::string realmArgument = {};
  int relayCount = 1;
  int simulatorCount = 0;
  int relayBasePort = 46001;
  int simulatorBasePort = 46101;
  uint32_t relayBaseNodeId = 10001;
  uint32_t simulatorBaseNodeId = 20001;
  int relayTicks = 0;
  int simulatorFrames = 0;
  int simulatorSleepMs = 16;
  float simulatorFrameTime = 1.0f / 60.0f;
  int launchDelayMs = 250;
  int runSeconds = 0;
  bool emitPlaceEvent = false;
};

struct SessionPaths
{
  std::filesystem::path sessionDir = {};
  std::filesystem::path realmDir = {};
};

struct ChildProcess
{
  std::string role = {};
  int index = 0;
  std::filesystem::path executablePath = {};
  std::filesystem::path configPath = {};
  std::filesystem::path logPath = {};
  std::filesystem::path realmDir = {};
  uint32_t nodeId = 0;
  int bindPort = 0;
  bool running = false;
  bool stoppedByLauncher = false;
  int exitCode = -1;
#if defined(_WIN32)
  HANDLE processHandle = nullptr;
  HANDLE threadHandle = nullptr;
  DWORD processId = 0;
#else
  pid_t processId = -1;
#endif
};

static std::atomic_bool gStopRequested = false;

static void HandleStopSignal(int)
{
  gStopRequested.store(true);
}

static std::string Trim(const std::string& value)
{
  size_t begin = 0;
  while (begin < value.size() && std::isspace((unsigned char)value[begin])) begin += 1;

  size_t end = value.size();
  while (end > begin && std::isspace((unsigned char)value[end - 1])) end -= 1;
  return value.substr(begin, end - begin);
}

static std::string FormatPath(const std::filesystem::path& path)
{
  return path.generic_string();
}

static std::string BuildTimestampString()
{
  const auto now = std::chrono::system_clock::now();
  const std::time_t rawNow = std::chrono::system_clock::to_time_t(now);
  std::tm timeInfo = {};
#if defined(_WIN32)
  localtime_s(&timeInfo, &rawNow);
#else
  localtime_r(&rawNow, &timeInfo);
#endif

  std::ostringstream stream = {};
  stream << std::put_time(&timeInfo, "%Y%m%d-%H%M%S");
  return stream.str();
}

static bool LoadJsonFile(const std::filesystem::path& path, nlohmann::json* json, std::string* errorMessage)
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

static bool SaveJsonFile(const std::filesystem::path& path, const nlohmann::json& json, std::string* errorMessage)
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

static nlohmann::json& EnsureObject(nlohmann::json& root, const char* key)
{
  if (!root.contains(key) || !root[key].is_object()) root[key] = nlohmann::json::object();
  return root[key];
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

static nlohmann::json& EnsureWalletObject(nlohmann::json& root)
{
  return EnsureObject(root, "wallet");
}

static nlohmann::json& EnsureRuntimeInterestObject(nlohmann::json& root)
{
  return EnsureObject(EnsureRuntimeObject(root), "interest");
}

static nlohmann::json& EnsureBindAddressObject(nlohmann::json& root)
{
  return EnsureObject(EnsureRuntimeObject(root), "bindAddress");
}

static void NormalizeConfigSchema(nlohmann::json* root)
{
  if (root == nullptr || !root->is_object()) return;
  EnsureWalletObject(*root);
  EnsureBindAddressObject(*root);
  EnsureRuntimeInterestObject(*root);
  EnsureSimulationObject(*root);
  EnsureServiceObject(*root);
}

static std::filesystem::path ResolveRealmDirectory(const std::filesystem::path& repoRoot, const std::string& realmArgument)
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

static bool ValidateRealmDirectory(const std::filesystem::path& realmDir, std::string* errorMessage)
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

static std::pair<int, int> BuildSimulatorInterestCoords(int index)
{
  return {index, 0};
}

static std::filesystem::path AppendExecutableSuffix(const std::filesystem::path& path)
{
#if defined(_WIN32)
  return path.string().ends_with(".exe") ? path : std::filesystem::path(path.string() + ".exe");
#else
  return path;
#endif
}

static std::string QuoteCommandArgument(const std::string& value)
{
  bool needsQuotes = value.empty();
  for (char ch : value)
  {
    if (std::isspace((unsigned char)ch) || ch == '"')
    {
      needsQuotes = true;
      break;
    }
  }

  if (!needsQuotes) return value;

  std::string result = '"' + value + '"';
  size_t position = 1;
  while ((position = result.find('"', position)) != std::string::npos)
  {
    if (position + 1 == result.size() - 1) break;
    result.insert(position, "\\");
    position += 2;
  }
  return result;
}

static std::string BuildCommandLine(const std::vector<std::string>& arguments)
{
  std::ostringstream stream = {};
  for (size_t i = 0; i < arguments.size(); ++i)
  {
    if (i > 0) stream << ' ';
    stream << QuoteCommandArgument(arguments[i]);
  }
  return stream.str();
}

static bool CreateSessionRealm(
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

static bool BuildRelayConfig(
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

static bool BuildSimulatorConfig(
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

static SessionPaths BuildSessionPaths(const std::filesystem::path& repoRoot)
{
  SessionPaths paths = {};
  const std::filesystem::path sessionRoot = repoRoot / "build" / "node_launcher";
  const std::string timestamp = BuildTimestampString();
  paths.sessionDir = sessionRoot / timestamp;
  paths.realmDir = paths.sessionDir / "realm";
  return paths;
}

static std::filesystem::path GetExecutableDirectory(const char* argv0)
{
  std::error_code error = {};
  std::filesystem::path executablePath = std::filesystem::absolute(argv0 == nullptr ? "" : argv0, error);
  if (error) executablePath = argv0 == nullptr ? std::filesystem::path() : std::filesystem::path(argv0);
  return executablePath.parent_path();
}

static bool ParseIntArgument(const std::string& text, int* value)
{
  if (value == nullptr) return false;
  char* end = nullptr;
  const long parsed = std::strtol(text.c_str(), &end, 10);
  if (end == text.c_str() || (end != nullptr && *end != '\0')) return false;
  *value = (int)parsed;
  return true;
}

static bool ParseU32Argument(const std::string& text, uint32_t* value)
{
  int parsed = 0;
  if (!ParseIntArgument(text, &parsed) || parsed < 0) return false;
  if (value != nullptr) *value = (uint32_t)parsed;
  return true;
}

static bool ParseFloatArgument(const std::string& text, float* value)
{
  if (value == nullptr) return false;
  char* end = nullptr;
  const float parsed = std::strtof(text.c_str(), &end);
  if (end == text.c_str() || (end != nullptr && *end != '\0')) return false;
  *value = parsed;
  return true;
}

static void PrintUsage()
{
  std::cout << "OpenRealm node launcher\n";
  std::cout << "Usage:\n";
  std::cout << "  openrealm-node-launcher --realm <name-or-path> [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  --relays <count>               Relay nodes to launch (default: 1)\n";
  std::cout << "  --simulators <count>           Simulator nodes to launch (default: 0)\n";
  std::cout << "  --config <path>                Base config.json to clone (default: config.json)\n";
  std::cout << "  --relay-base-port <port>       First relay UDP port (default: 46001)\n";
  std::cout << "  --sim-base-port <port>         First simulator UDP port (default: 46101)\n";
  std::cout << "  --relay-base-node-id <id>      First relay node id (default: 10001)\n";
  std::cout << "  --sim-base-node-id <id>        First simulator node id (default: 20001)\n";
  std::cout << "  --relay-ticks <count>          Relay service ticks, 0 = infinite (default: 0)\n";
  std::cout << "  --sim-frames <count>           Simulator frames, 0 = infinite (default: 0)\n";
  std::cout << "  --sim-sleep-ms <ms>            Simulator frame sleep (default: 16)\n";
  std::cout << "  --sim-frame-time <seconds>     Simulator timestep (default: 0.0166667)\n";
  std::cout << "  --launch-delay-ms <ms>         Delay between child launches (default: 250)\n";
  std::cout << "  --run-seconds <seconds>        Auto-stop after N seconds, 0 = wait for Ctrl+C\n";
  std::cout << "  --emit-place-event             Let the first simulator emit the startup test place event\n";
  std::cout << "  --help                         Show this help text\n";
}

static bool ParseArguments(int argc, char** argv, LaunchOptions* options, std::string* errorMessage)
{
  if (options == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "launch options output was null";
    return false;
  }

  for (int i = 1; i < argc; ++i)
  {
    const std::string argument = argv[i] == nullptr ? std::string() : std::string(argv[i]);
    if (argument == "--help")
    {
      PrintUsage();
      return false;
    }

    auto requireValue = [&](const char* label, std::string* value)
    {
      if (i + 1 >= argc)
      {
        if (errorMessage != nullptr) *errorMessage = std::string("missing value for ") + label;
        return false;
      }
      *value = argv[++i] == nullptr ? std::string() : std::string(argv[i]);
      return true;
    };

    std::string value = {};
    if (argument == "--realm")
    {
      if (!requireValue("--realm", &value)) return false;
      options->realmArgument = value;
      continue;
    }

    if (argument == "--config")
    {
      if (!requireValue("--config", &value)) return false;
      options->baseConfigPath = value;
      continue;
    }

    if (argument == "--relays")
    {
      if (!requireValue("--relays", &value) || !ParseIntArgument(value, &options->relayCount))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--relays must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--simulators")
    {
      if (!requireValue("--simulators", &value) || !ParseIntArgument(value, &options->simulatorCount))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--simulators must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--relay-base-port")
    {
      if (!requireValue("--relay-base-port", &value) || !ParseIntArgument(value, &options->relayBasePort))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--relay-base-port must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--sim-base-port")
    {
      if (!requireValue("--sim-base-port", &value) || !ParseIntArgument(value, &options->simulatorBasePort))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--sim-base-port must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--relay-base-node-id")
    {
      if (!requireValue("--relay-base-node-id", &value) || !ParseU32Argument(value, &options->relayBaseNodeId))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--relay-base-node-id must be a non-negative integer";
        return false;
      }
      continue;
    }

    if (argument == "--sim-base-node-id")
    {
      if (!requireValue("--sim-base-node-id", &value) || !ParseU32Argument(value, &options->simulatorBaseNodeId))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--sim-base-node-id must be a non-negative integer";
        return false;
      }
      continue;
    }

    if (argument == "--relay-ticks")
    {
      if (!requireValue("--relay-ticks", &value) || !ParseIntArgument(value, &options->relayTicks))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--relay-ticks must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--sim-frames")
    {
      if (!requireValue("--sim-frames", &value) || !ParseIntArgument(value, &options->simulatorFrames))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--sim-frames must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--sim-sleep-ms")
    {
      if (!requireValue("--sim-sleep-ms", &value) || !ParseIntArgument(value, &options->simulatorSleepMs))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--sim-sleep-ms must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--sim-frame-time")
    {
      if (!requireValue("--sim-frame-time", &value) || !ParseFloatArgument(value, &options->simulatorFrameTime))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--sim-frame-time must be a number";
        return false;
      }
      continue;
    }

    if (argument == "--launch-delay-ms")
    {
      if (!requireValue("--launch-delay-ms", &value) || !ParseIntArgument(value, &options->launchDelayMs))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--launch-delay-ms must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--run-seconds")
    {
      if (!requireValue("--run-seconds", &value) || !ParseIntArgument(value, &options->runSeconds))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--run-seconds must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--emit-place-event")
    {
      options->emitPlaceEvent = true;
      continue;
    }

    if (errorMessage != nullptr) *errorMessage = "unknown argument: " + argument;
    return false;
  }

  if (options->realmArgument.empty())
  {
    if (errorMessage != nullptr) *errorMessage = "--realm is required";
    return false;
  }

  if (options->relayCount < 0 || options->simulatorCount < 0)
  {
    if (errorMessage != nullptr) *errorMessage = "node counts cannot be negative";
    return false;
  }

  if (options->relayCount == 0 && options->simulatorCount == 0)
  {
    if (errorMessage != nullptr) *errorMessage = "at least one relay or simulator must be launched";
    return false;
  }

  if (options->relayBasePort < 1 || options->relayBasePort > 65535 || options->simulatorBasePort < 1 || options->simulatorBasePort > 65535)
  {
    if (errorMessage != nullptr) *errorMessage = "base ports must be between 1 and 65535";
    return false;
  }

  if (options->launchDelayMs < 0 || options->runSeconds < 0)
  {
    if (errorMessage != nullptr) *errorMessage = "delay and runtime cannot be negative";
    return false;
  }

  return true;
}

static bool PollChildProcess(ChildProcess* child)
{
  if (child == nullptr || !child->running) return false;
#if defined(_WIN32)
  DWORD exitCode = STILL_ACTIVE;
  if (!GetExitCodeProcess(child->processHandle, &exitCode)) return false;
  if (exitCode == STILL_ACTIVE) return true;
  child->running = false;
  child->exitCode = (int)exitCode;
  return false;
#else
  int status = 0;
  const pid_t result = waitpid(child->processId, &status, WNOHANG);
  if (result == 0) return true;
  if (result < 0) return false;
  child->running = false;
  child->exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  return false;
#endif
}

static void CloseChildProcess(ChildProcess* child)
{
  if (child == nullptr) return;
#if defined(_WIN32)
  if (child->threadHandle != nullptr)
  {
    CloseHandle(child->threadHandle);
    child->threadHandle = nullptr;
  }
  if (child->processHandle != nullptr)
  {
    CloseHandle(child->processHandle);
    child->processHandle = nullptr;
  }
#endif
}

static void StopChildProcess(ChildProcess* child)
{
  if (child == nullptr || !child->running) return;
  child->stoppedByLauncher = true;
#if defined(_WIN32)
  TerminateProcess(child->processHandle, 1);
  WaitForSingleObject(child->processHandle, 5000);
  DWORD exitCode = 1;
  GetExitCodeProcess(child->processHandle, &exitCode);
  child->running = false;
  child->exitCode = (int)exitCode;
#else
  kill(child->processId, SIGTERM);
  int status = 0;
  waitpid(child->processId, &status, 0);
  child->running = false;
  child->exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

static bool LaunchChildProcess(
    const std::filesystem::path& repoRoot,
    const std::filesystem::path& executablePath,
    const std::filesystem::path& configPath,
    const std::filesystem::path& realmDir,
    ChildProcess* child,
    std::string* errorMessage)
{
  if (child == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "child output was null";
    return false;
  }

  const std::vector<std::string> arguments = {
      FormatPath(executablePath),
      "--config",
      FormatPath(configPath),
      "--realm-dir",
      FormatPath(realmDir),
      "--no-cli",
  };

#if defined(_WIN32)
  SECURITY_ATTRIBUTES securityAttributes = {};
  securityAttributes.nLength = sizeof(securityAttributes);
  securityAttributes.bInheritHandle = TRUE;

  HANDLE inputHandle = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &securityAttributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  HANDLE logHandle = CreateFileA(
      FormatPath(child->logPath).c_str(),
      GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      &securityAttributes,
      CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      nullptr);
  if (inputHandle == INVALID_HANDLE_VALUE || logHandle == INVALID_HANDLE_VALUE)
  {
    if (inputHandle != INVALID_HANDLE_VALUE) CloseHandle(inputHandle);
    if (logHandle != INVALID_HANDLE_VALUE) CloseHandle(logHandle);
    if (errorMessage != nullptr) *errorMessage = "failed to open child stdio handles for " + child->role;
    return false;
  }

  STARTUPINFOA startupInfo = {};
  startupInfo.cb = sizeof(startupInfo);
  startupInfo.dwFlags = STARTF_USESTDHANDLES;
  startupInfo.hStdInput = inputHandle;
  startupInfo.hStdOutput = logHandle;
  startupInfo.hStdError = logHandle;

  PROCESS_INFORMATION processInfo = {};
  std::string commandLine = BuildCommandLine(arguments);
  std::vector<char> commandBuffer(commandLine.begin(), commandLine.end());
  commandBuffer.push_back('\0');

  const BOOL started = CreateProcessA(
      nullptr,
      commandBuffer.data(),
      nullptr,
      nullptr,
      TRUE,
      CREATE_NO_WINDOW,
      nullptr,
      FormatPath(repoRoot).c_str(),
      &startupInfo,
      &processInfo);

  CloseHandle(inputHandle);
  CloseHandle(logHandle);

  if (!started)
  {
    if (errorMessage != nullptr) *errorMessage = "failed to launch " + child->role + " from " + FormatPath(executablePath);
    return false;
  }

  child->running = true;
  child->processHandle = processInfo.hProcess;
  child->threadHandle = processInfo.hThread;
  child->processId = processInfo.dwProcessId;
  return true;
#else
  const pid_t pid = fork();
  if (pid < 0)
  {
    if (errorMessage != nullptr) *errorMessage = "fork failed for " + child->role;
    return false;
  }

  if (pid == 0)
  {
    const int inputFd = open("/dev/null", O_RDONLY);
    const int logFd = open(child->logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (inputFd >= 0) dup2(inputFd, STDIN_FILENO);
    if (logFd >= 0)
    {
      dup2(logFd, STDOUT_FILENO);
      dup2(logFd, STDERR_FILENO);
    }
    chdir(repoRoot.c_str());

    std::vector<char*> execArgs = {};
    for (const std::string& argument : arguments)
    {
      execArgs.push_back(const_cast<char*>(argument.c_str()));
    }
    execArgs.push_back(nullptr);
    execvp(execArgs[0], execArgs.data());
    std::_Exit(127);
  }

  child->running = true;
  child->processId = pid;
  return true;
#endif
}

static void PrintLaunchLine(const ChildProcess& child)
{
  std::cout << "- launched " << child.role << ' ' << child.index
            << ": pid="
#if defined(_WIN32)
            << child.processId
#else
            << child.processId
#endif
            << ", nodeId=" << child.nodeId
            << ", bind=127.0.0.1:" << child.bindPort
            << ", config=" << FormatPath(child.configPath)
            << ", log=" << FormatPath(child.logPath) << "\n";
}

static void PrintStatusLines(const std::vector<ChildProcess>& children)
{
  for (const ChildProcess& child : children)
  {
    std::cout << "- " << child.role << ' ' << child.index
              << ": " << (child.running ? "running" : "stopped")
              << ", exit=" << child.exitCode
              << ", log=" << FormatPath(child.logPath) << "\n";
  }
}

int main(int argc, char** argv)
{
  std::signal(SIGINT, HandleStopSignal);
  std::signal(SIGTERM, HandleStopSignal);

  LaunchOptions options = {};
  options.repoRoot = std::filesystem::current_path();

  std::string error = {};
  if (!ParseArguments(argc, argv, &options, &error))
  {
    if (!error.empty()) std::cout << "OpenRealm node launcher\n- error: " << error << "\n";
    return error.empty() ? 0 : 1;
  }

  const std::filesystem::path baseConfigPath = options.baseConfigPath.is_absolute()
                                                   ? options.baseConfigPath
                                                   : options.repoRoot / options.baseConfigPath;
  const std::filesystem::path sourceRealmDir = ResolveRealmDirectory(options.repoRoot, options.realmArgument);
  if (!ValidateRealmDirectory(sourceRealmDir, &error))
  {
    std::cout << "OpenRealm node launcher\n- error: " << error << "\n";
    return 1;
  }

  nlohmann::json baseConfig = {};
  if (!LoadJsonFile(baseConfigPath, &baseConfig, &error))
  {
    std::cout << "OpenRealm node launcher\n- error: " << error << "\n";
    return 1;
  }
  NormalizeConfigSchema(&baseConfig);

  const std::filesystem::path executableDir = GetExecutableDirectory(argv[0]);
  const std::filesystem::path relayExecutable = AppendExecutableSuffix(executableDir / "openrealm-relay");
  const std::filesystem::path simulatorExecutable = AppendExecutableSuffix(executableDir / "openrealm-simulator");

  if (options.relayCount > 0 && !std::filesystem::exists(relayExecutable))
  {
    std::cout << "OpenRealm node launcher\n- error: relay executable not found: " << FormatPath(relayExecutable) << "\n";
    return 1;
  }

  if (options.simulatorCount > 0 && !std::filesystem::exists(simulatorExecutable))
  {
    std::cout << "OpenRealm node launcher\n- error: simulator executable not found: " << FormatPath(simulatorExecutable) << "\n";
    return 1;
  }

  const SessionPaths sessionPaths = BuildSessionPaths(options.repoRoot);
  std::filesystem::create_directories(sessionPaths.sessionDir);

  std::vector<int> relayPorts = {};
  for (int i = 0; i < options.relayCount; ++i)
  {
    relayPorts.push_back(options.relayBasePort + i);
  }

  if (!CreateSessionRealm(sourceRealmDir, sessionPaths.realmDir, relayPorts, &error))
  {
    std::cout << "OpenRealm node launcher\n- error: " << error << "\n";
    return 1;
  }

  std::vector<ChildProcess> children = {};
  children.reserve((size_t)(options.relayCount + options.simulatorCount));

  for (int i = 0; i < options.relayCount; ++i)
  {
    ChildProcess child = {};
    child.role = "relay";
    child.index = i + 1;
    child.executablePath = relayExecutable;
    child.configPath = sessionPaths.sessionDir / ("relay-" + std::to_string(i + 1) + ".json");
    child.logPath = sessionPaths.sessionDir / ("relay-" + std::to_string(i + 1) + ".log");
    child.realmDir = sessionPaths.realmDir;
    child.nodeId = options.relayBaseNodeId + (uint32_t)i;
    child.bindPort = options.relayBasePort + i;

    if (!BuildRelayConfig(baseConfig, options, sessionPaths.realmDir, i, child.configPath, &error))
    {
      std::cout << "OpenRealm node launcher\n- error: " << error << "\n";
      return 1;
    }

    if (!LaunchChildProcess(options.repoRoot, child.executablePath, child.configPath, child.realmDir, &child, &error))
    {
      std::cout << "OpenRealm node launcher\n- error: " << error << "\n";
      for (ChildProcess& launchedChild : children)
      {
        StopChildProcess(&launchedChild);
        CloseChildProcess(&launchedChild);
      }
      return 1;
    }

    children.push_back(child);
    PrintLaunchLine(children.back());
    if (options.launchDelayMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(options.launchDelayMs));
  }

  for (int i = 0; i < options.simulatorCount; ++i)
  {
    ChildProcess child = {};
    child.role = "simulator";
    child.index = i + 1;
    child.executablePath = simulatorExecutable;
    child.configPath = sessionPaths.sessionDir / ("simulator-" + std::to_string(i + 1) + ".json");
    child.logPath = sessionPaths.sessionDir / ("simulator-" + std::to_string(i + 1) + ".log");
    child.realmDir = sessionPaths.realmDir;
    child.nodeId = options.simulatorBaseNodeId + (uint32_t)i;
    child.bindPort = options.simulatorBasePort + i;

    if (!BuildSimulatorConfig(baseConfig, options, sessionPaths.realmDir, i, options.relayCount, child.configPath, &error))
    {
      std::cout << "OpenRealm node launcher\n- error: " << error << "\n";
      for (ChildProcess& launchedChild : children)
      {
        StopChildProcess(&launchedChild);
        CloseChildProcess(&launchedChild);
      }
      return 1;
    }

    if (!LaunchChildProcess(options.repoRoot, child.executablePath, child.configPath, child.realmDir, &child, &error))
    {
      std::cout << "OpenRealm node launcher\n- error: " << error << "\n";
      for (ChildProcess& launchedChild : children)
      {
        StopChildProcess(&launchedChild);
        CloseChildProcess(&launchedChild);
      }
      return 1;
    }

    children.push_back(child);
    PrintLaunchLine(children.back());
    if (options.launchDelayMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(options.launchDelayMs));
  }

  std::cout << "OpenRealm node launcher\n";
  std::cout << "- repo root: " << FormatPath(options.repoRoot) << "\n";
  std::cout << "- base config: " << FormatPath(baseConfigPath) << "\n";
  std::cout << "- source realm: " << FormatPath(sourceRealmDir) << "\n";
  std::cout << "- session dir: " << FormatPath(sessionPaths.sessionDir) << "\n";
  std::cout << "- session realm: " << FormatPath(sessionPaths.realmDir) << "\n";
  std::cout << "- relays requested: " << options.relayCount << "\n";
  std::cout << "- simulators requested: " << options.simulatorCount << "\n";
  std::cout << "- auto stop seconds: " << options.runSeconds << "\n";
  std::cout << "- stop hint: press Ctrl+C to stop every launched node\n";

  const auto startTime = std::chrono::steady_clock::now();
  auto lastStatusPrint = startTime;
  bool stopRequested = false;

  while (!stopRequested)
  {
    bool anyRunning = false;
    for (ChildProcess& child : children)
    {
      if (PollChildProcess(&child)) anyRunning = true;
    }

    if (!anyRunning) break;
    if (gStopRequested.load())
    {
      std::cout << "- stop request: signal received\n";
      stopRequested = true;
      break;
    }

    if (options.runSeconds > 0)
    {
      const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startTime).count();
      if (elapsed >= options.runSeconds)
      {
        std::cout << "- stop request: auto-stop reached after " << elapsed << " seconds\n";
        stopRequested = true;
        break;
      }
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - lastStatusPrint >= std::chrono::seconds(5))
    {
      PrintStatusLines(children);
      lastStatusPrint = now;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  for (ChildProcess& child : children)
  {
    StopChildProcess(&child);
    PollChildProcess(&child);
  }

  std::cout << "- final status:\n";
  PrintStatusLines(children);

  int failedChildren = 0;
  for (ChildProcess& child : children)
  {
    if (child.exitCode != 0 && !child.stoppedByLauncher) failedChildren += 1;
    CloseChildProcess(&child);
  }

  std::cout << "- session logs: " << FormatPath(sessionPaths.sessionDir) << "\n";
  return failedChildren == 0 ? 0 : 1;
}
