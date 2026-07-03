#pragma once

#include "../blockchain/BlockchainConfig.h"
#include "../blockchain/GlobalParamsContract.h"

#include <cstdint>
#include <string>

struct RuntimeRealmState
{
  std::string chainId = {};
  BlockchainConfig blockchainConfig = {};
  GlobalParamsState globalParams = {};
};

std::string BuildRuntimeRealmFingerprint(const RuntimeRealmState& realmState);
uint64_t ComputeRuntimeRealmHash(const RuntimeRealmState& realmState);
