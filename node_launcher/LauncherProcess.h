#pragma once

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

#include "LauncherTypes.h"

extern std::atomic_bool gStopRequested;

void HandleStopSignal(int signal);
bool PollChildProcess(ChildProcess* child);
void CloseChildProcess(ChildProcess* child);
void StopChildProcess(ChildProcess* child);
bool LaunchChildProcess(
    const std::filesystem::path& repoRoot,
    const std::filesystem::path& executablePath,
    const std::filesystem::path& configPath,
    const std::filesystem::path& realmDir,
    ChildProcess* child,
    std::string* errorMessage);
void PrintLaunchLine(const ChildProcess& child);
void PrintStatusLines(const std::vector<ChildProcess>& children);
