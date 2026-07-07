#pragma once

#include "ProtocolVersion.h"

#include <string>

struct BlockchainConfig
{
  uint32_t protocolVersion = kBlockchainProtocolVersion;
  std::string rpcUrl = {};
  std::string globalParamsAddress = {};
  std::string playerRegistryAddress = {};
  std::string chunkClaimsAddress = {};
  std::string marketplaceAddress = {};
  int connectionTimeoutSeconds = 1;
  int readTimeoutSeconds = 1;
  int writeTimeoutSeconds = 1;
};
