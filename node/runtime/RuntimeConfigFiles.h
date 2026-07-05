#pragma once

#include "../blockchain/BlockchainConfig.h"
#include "../blockchain/Wallet.h"
#include "RuntimeClient.h"

#include <string>
#include <vector>

struct RuntimeJumpNodeState
{
  RuntimePeerAddress peerAddress = {};
  std::string label = {};
};

struct RuntimeRealmFiles
{
  std::string directory = {};
  std::string realmName = {};
  BlockchainConfig blockchainConfig = {};
  std::vector<RuntimeJumpNodeState> jumpNodes = {};
};

struct RuntimeNodeFilesConfig
{
  std::string configPath = "config.json";
  std::string selectedRealm = "realms/test";
  Wallet wallet = {};

  RuntimePeerAddress simulatorBindAddress = {"127.0.0.1", 46010};
  int simulatorJumpNodeIndex = 0;
  uint32_t simulatorNodeId = 7001;
  int simulatorInterestChunkX = 0;
  int simulatorInterestChunkY = 0;
  uint32_t simulatorInterestRadius = 1;
  uint8_t simulatorPlaceVoxelValue = 255;
  uint32_t simulatorReceiveTimeoutMs = 1000;
  float simulatorFrameTime = 1.0f / 60.0f;
  int simulatorSleepMs = 16;

  RuntimePeerAddress relayBindAddress = {"127.0.0.1", 46001};
  uint32_t relayNodeId = 9001;
  uint32_t relayReceiveTimeoutMs = 100;
  int relayTicks = 0;
};

bool LoadRuntimeNodeFilesConfig(const std::string& configPath, RuntimeNodeFilesConfig* config, std::string* errorMessage);
bool LoadRuntimeRealmFiles(const std::string& realmDirectory, RuntimeRealmFiles* realmFiles, std::string* errorMessage);
