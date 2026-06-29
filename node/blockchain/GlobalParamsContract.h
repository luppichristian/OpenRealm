#pragma once

#include "SmartContract.h"

#include <cstdint>
#include <string>

struct GlobalParamsState
{
  bool available = false;
  int32_t minChunkCoord = 0;
  int32_t maxChunkCoord = 0;
  std::string minChunkPrice = {};
  uint64_t maxFeeBps = 0;
  uint64_t minAuctionDuration = 0;
};

class GlobalParamsContract : public SmartContract
{
 public:
  using SmartContract::SmartContract;

  GlobalParamsState GetState() const;

 protected:
  const char* GetContractName() const override;
};
