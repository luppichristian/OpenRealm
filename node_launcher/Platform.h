#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "LauncherTypes.h"

bool PollPlatformChildProcess(ChildProcess* child);
void ClosePlatformChildProcess(ChildProcess* child);
void StopPlatformChildProcess(ChildProcess* child);
bool LaunchPlatformChildProcess(
    const std::filesystem::path& repoRoot,
    const std::filesystem::path& executablePath,
    const std::filesystem::path& configPath,
    const std::filesystem::path& realmDir,
    const std::vector<std::string>& arguments,
    ChildProcess* child,
    std::string* errorMessage);
