#pragma once

#include "blockchain/Blockchain.h"
#include "blockchain/BlockchainConfig.h"
#include "runtime/RuntimeRealm.h"

#include <string>

struct NodeBootConfig
{
  std::string configPath = "config.json";
  std::string realmDirectory = {};
};

NodeBootConfig ParseNodeBootConfig(int argc, char** argv);
RuntimeRealmState BuildNodeRuntimeRealmState(Blockchain& blockchain, const BlockchainConfig& blockchainConfig);
