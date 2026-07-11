#pragma once

#include "../node/runtime/RuntimeClient.h"
#include "../node/runtime/RuntimeWorldPosition.h"

#include <cstdint>
#include <string>

struct TesterOptions
{
  std::string configPath = "config.json";
  std::string realmDirectory = "realms/test";
  int jumpNodeIndex = 0;
  RuntimePeerAddress bindAddress = {"127.0.0.1", 46301};
  RuntimeWorldPosition joinTargetPosition = {};
  uint32_t runSeconds = 8;
  size_t expectMinTopologyNodes = 1;
  bool requirePlayerSnapshot = false;
};

bool ParseTesterArguments(int argc, char** argv, TesterOptions* options, std::string* errorMessage);
void PrintTesterUsage();
