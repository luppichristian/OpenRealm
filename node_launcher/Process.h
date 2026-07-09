#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "LauncherTypes.h"

void HandleStopSignal(int signal);
bool IsStopRequested();

bool LaunchChildProcess(
    const std::filesystem::path& repoRoot,
    const std::filesystem::path& executablePath,
    const std::filesystem::path& configPath,
    const std::filesystem::path& realmDir,
    int jumpNodeIndex,
    ChildProcess* child,
    std::string* errorMessage);
bool PollChildProcess(ChildProcess* child);
void CloseChildProcess(ChildProcess* child);
void StopChildProcess(ChildProcess* child);
void StopAndCloseChildren(std::vector<ChildProcess>* children);
void PrintLaunchLine(const ChildProcess& child);
void PrintStatusLines(const std::vector<ChildProcess>& children);
