#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "LauncherTypes.h"

bool LoadJsonFile(const std::filesystem::path& path, nlohmann::json* json, std::string* errorMessage);
bool SaveJsonFile(const std::filesystem::path& path, const nlohmann::json& json, std::string* errorMessage);
void NormalizeConfigSchema(nlohmann::json* root);
std::filesystem::path ResolveRealmDirectory(const std::filesystem::path& repoRoot, const std::string& realmArgument);
bool ValidateRealmDirectory(const std::filesystem::path& realmDir, std::string* errorMessage);
bool CreateSessionRealm(
    const std::filesystem::path& sourceRealmDir,
    const std::filesystem::path& sessionRealmDir,
    const std::vector<int>& relayPorts,
    std::string* errorMessage);
bool BuildRelayConfig(
    const nlohmann::json& baseConfig,
    const LaunchOptions& options,
    const std::filesystem::path& realmDir,
    int relayIndex,
    const std::filesystem::path& outputPath,
    std::string* errorMessage);
bool BuildSimulatorConfig(
    const nlohmann::json& baseConfig,
    const LaunchOptions& options,
    const std::filesystem::path& realmDir,
    int simulatorIndex,
    int launchedRelayCount,
    const std::filesystem::path& outputPath,
    std::string* errorMessage);
