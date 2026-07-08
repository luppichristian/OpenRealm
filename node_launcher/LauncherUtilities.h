#pragma once

#include <filesystem>
#include <string>
#include <utility>

#include "LauncherTypes.h"

std::string Trim(const std::string& value);
std::string FormatPath(const std::filesystem::path& path);
std::string BuildTimestampString();
std::pair<int, int> BuildSimulatorInterestCoords(int index);
std::filesystem::path AppendExecutableSuffix(const std::filesystem::path& path);
SessionPaths BuildSessionPaths(const std::filesystem::path& repoRoot);
std::filesystem::path GetExecutableDirectory(const char* argv0);
