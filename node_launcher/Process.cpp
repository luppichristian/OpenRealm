#include "Process.h"

#include <atomic>
#include <iostream>

#include "LauncherUtilities.h"
#include "Platform.h"

namespace
{
std::atomic_bool gStopRequested = false;
}

void HandleStopSignal(int)
{
  gStopRequested.store(true);
}

bool IsStopRequested()
{
  return gStopRequested.load();
}

bool LaunchChildProcess(
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
  return LaunchPlatformChildProcess(repoRoot, arguments, child, errorMessage);
}

bool PollChildProcess(ChildProcess* child)
{
  return PollPlatformChildProcess(child);
}

void CloseChildProcess(ChildProcess* child)
{
  ClosePlatformChildProcess(child);
}

void StopChildProcess(ChildProcess* child)
{
  StopPlatformChildProcess(child);
}

void StopAndCloseChildren(std::vector<ChildProcess>* children)
{
  if (children == nullptr) return;
  for (ChildProcess& child : *children)
  {
    StopChildProcess(&child);
    CloseChildProcess(&child);
  }
}

void PrintLaunchLine(const ChildProcess& child)
{
  std::cout << "- launched " << child.role << ' ' << child.index
            << ": pid=" << child.processId << ", nodeId=" << child.nodeId
            << ", bind=127.0.0.1:" << child.bindPort << ", config=" << FormatPath(child.configPath)
            << ", log=" << FormatPath(child.logPath) << "\n";
}

void PrintStatusLines(const std::vector<ChildProcess>& children)
{
  for (const ChildProcess& child : children)
  {
    std::cout << "- " << child.role << ' ' << child.index
              << ": " << (child.running ? "running" : "stopped")
              << ", exit=" << child.exitCode
              << ", log=" << FormatPath(child.logPath) << "\n";
  }
}
