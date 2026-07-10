#pragma once

#include "../runtime/RuntimeWorldPosition.h"

#include <string>

struct ClientLaunchArgs
{
  std::string configPath = "config.json";
  std::string realmDirectory = {};
  int jumpNodeIndex = 0;
  RuntimeWorldPosition joinTargetPosition = {};
  bool autoPlay = false;
};

void PrintClientUsage();
bool ParseClientLaunchArgs(int argc, char** argv, ClientLaunchArgs* options, std::string* errorMessage);
