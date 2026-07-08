#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "LauncherTypes.h"

bool LaunchPlatformChildProcess(
    const std::filesystem::path& repoRoot,
    const std::vector<std::string>& arguments,
    ChildProcess* child,
    std::string* errorMessage);
bool PollPlatformChildProcess(ChildProcess* child);
void ClosePlatformChildProcess(ChildProcess* child);
void StopPlatformChildProcess(ChildProcess* child);
