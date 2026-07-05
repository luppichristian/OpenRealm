#pragma once

#include "../blockchain/Wallet.h"
#include "RuntimeClient.h"

#include <cstdint>
#include <string>

struct NodeFilesConfig
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
  int simulatorFrames = 0;
  float simulatorFrameTime = 1.0f / 60.0f;
  int simulatorSleepMs = 16;
  bool simulatorRuntimeEnabled = false;
  bool simulatorEmitPlaceEvent = false;

  RuntimePeerAddress relayBindAddress = {"127.0.0.1", 46001};
  uint32_t relayNodeId = 9001;
  uint32_t relayReceiveTimeoutMs = 100;
  int relayTicks = 0;
};

struct NodeBootConfig
{
  std::string configPath = "config.json";
  std::string realmDirectory = {};
};

NodeBootConfig ParseNodeBootConfig(int argc, char** argv);
bool LoadNodeFilesConfig(const std::string& configPath, NodeFilesConfig* config, std::string* errorMessage);
