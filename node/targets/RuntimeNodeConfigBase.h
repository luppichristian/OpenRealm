#pragma once

#include "../blockchain/BlockchainConfig.h"
#include "../blockchain/Wallet.h"
#include "../runtime/NodeConfigFiles.h"

#include <cstddef>
#include <cstdint>
#include <string>

struct RuntimeNodeConfigBase
{
  RuntimePeerAddress bindAddress = {"127.0.0.1", 46010};
  uint32_t receiveTimeoutMs = DEFAULT_RUNTIME_RECEIVE_TIMEOUT_MS;
  size_t maxNodeConnections = DEFAULT_RUNTIME_MAX_NODE_CONNECTIONS;
  std::string configPath = "config.json";
  std::string realmDirectory = "realms/test";
  std::string realmName = {};
  size_t jumpNodeCount = 0;
  BlockchainConfig blockchainConfig = {};
  Wallet wallet = {};
};
