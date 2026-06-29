#pragma once

#include "SmartContract.h"

#include <cstdint>
#include <string>

struct SaleState
{
  bool available = false;
  uint8_t saleKind = 0;
  std::string saleId = {};
  std::string tokenId = {};
  int32_t x = 0;
  int32_t y = 0;
  std::string seller = {};
  bool sellerStillOwnsChunk = false;
  bool active = false;
  std::string price = {};
  std::string reservePrice = {};
  std::string minBidIncrement = {};
  uint64_t endTime = 0;
  std::string highestBidder = {};
  std::string highestBid = {};
};

class MarketplaceContract : public SmartContract
{
 public:
  using SmartContract::SmartContract;

  uint64_t GetFeeBps() const;
  SaleState GetSaleStateForChunk(int32_t x, int32_t y) const;
  SaleState GetSaleStateForToken(uint64_t tokenId) const;

 protected:
  const char* GetContractName() const override;
};
