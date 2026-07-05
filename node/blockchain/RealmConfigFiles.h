#pragma once

#include "BlockchainConfig.h"
#include "Blockchain.h"
#include "../runtime/RuntimeClient.h"
#include "../runtime/RuntimeRealm.h"

#include <string>
#include <vector>

struct RealmJumpNodeState
{
  RuntimePeerAddress peerAddress = {};
  std::string label = {};
};

struct RealmConfigFiles
{
  std::string directory = {};
  std::string realmName = {};
  BlockchainConfig blockchainConfig = {};
  std::vector<RealmJumpNodeState> jumpNodes = {};
};

bool LoadRealmConfigFiles(const std::string& realmDirectory, RealmConfigFiles* realmFiles, std::string* errorMessage);
RuntimeRealmState BuildRuntimeRealmState(Blockchain& blockchain, const BlockchainConfig& blockchainConfig);
