#include "App.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "Args.h"
#include "Config.h"
#include "LauncherUtilities.h"
#include "Process.h"

int RunApp(int argc, char** argv)
{
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
    child.bindPort = options.relayBasePort + i;
    child.jumpNodeIndex = 0;

    if (!BuildRelayConfig(baseConfig, options, sessionPaths.realmDir, i, child.configPath, &error))
    {
      std::cout << "OpenRealm node launcher\n- error: " << error << "\n";
      StopAndCloseChildren(&children);
      return 1;
    }

    if (!LaunchChildProcess(options.repoRoot, child.executablePath, child.configPath, child.realmDir, child.jumpNodeIndex, &child, &error))
    {
      std::cout << "OpenRealm node launcher\n- error: " << error << "\n";
      StopAndCloseChildren(&children);
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
    child.bindPort = options.simulatorBasePort + i;
    child.jumpNodeIndex = options.relayCount > 0 ? (i % options.relayCount) : 0;

    if (!BuildSimulatorConfig(baseConfig, options, sessionPaths.realmDir, i, options.relayCount, child.configPath, &error))
    {
      std::cout << "OpenRealm node launcher\n- error: " << error << "\n";
      StopAndCloseChildren(&children);
      return 1;
    }

    if (!LaunchChildProcess(options.repoRoot, child.executablePath, child.configPath, child.realmDir, child.jumpNodeIndex, &child, &error))
    {
      std::cout << "OpenRealm node launcher\n- error: " << error << "\n";
      StopAndCloseChildren(&children);
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
    if (IsStopRequested())
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
