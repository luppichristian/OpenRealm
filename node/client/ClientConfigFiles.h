#pragma once

#include "../runtime/RuntimeWorldPosition.h"

#include <string>
#include <vector>

struct ClientJumpNodeState
{
  std::string host = {};
  int port = 0;
  std::string label = {};
};

struct ClientRealmState
{
  std::string directory = {};
  std::string realmName = {};
  std::vector<ClientJumpNodeState> jumpNodes = {};
};

struct ClientFilesConfig
{
  std::string configPath = "config.json";
  std::string selectedRealm = "realms/test";
  int jumpNodeIndex = 0;
  RuntimeWorldPosition joinTargetPosition = {};
  float masterVolume = 1.0f;
  float mouseSensitivity = 1.0f;
  bool invertMouseY = false;
  bool showFps = true;
};

bool LoadClientFilesConfig(const std::string& configPath, ClientFilesConfig* config, std::string* errorMessage);
bool SaveClientFilesConfig(const ClientFilesConfig& config, std::string* errorMessage);
std::vector<std::string> DiscoverClientRealmDirectories();
bool LoadClientRealmState(const std::string& realmDirectory, ClientRealmState* realmState, std::string* errorMessage);
