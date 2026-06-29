#pragma once

#include <string>

struct BlockchainConfig
{
  std::string rpcUrl = {};
  std::string globalParamsAddress = {};
  std::string playerRegistryAddress = {};
  std::string chunkClaimsAddress = {};
  std::string marketplaceAddress = {};
  int connectionTimeoutSeconds = 1;
  int readTimeoutSeconds = 1;
  int writeTimeoutSeconds = 1;
};
