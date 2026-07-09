#pragma once

#include "SmartContract.h"

#include <cstdint>
#include <string>

struct ListingState
{
  bool available = false;
  std::string id = {};
  std::string tokenId = {};
  int32_t x = 0;
  int32_t y = 0;
  std::string seller = {};
  std::string price = {};
  bool active = false;
};

struct AuctionState
{
  bool available = false;
  std::string id = {};
  std::string tokenId = {};
  int32_t x = 0;
  int32_t y = 0;
  std::string seller = {};
  std::string reservePrice = {};
  std::string minBidIncrement = {};
  uint64_t endTime = 0;
  std::string highestBidder = {};
  std::string highestBid = {};
  bool active = false;
  bool settled = false;
};

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

struct MarketplaceListingTransaction
{
  std::string listingId = {};
  BlockchainTransactionReceipt receipt = {};
};

struct MarketplaceAuctionTransaction
{
  std::string auctionId = {};
  BlockchainTransactionReceipt receipt = {};
};

class MarketplaceContract : public SmartContract
{
 public:
  using SmartContract::SmartContract;

  std::string GetOwner() const;
  BlockchainTransactionReceipt TransferOwnership(const std::string& newOwner) const;
  uint64_t GetSaleKindNone() const;
  uint64_t GetSaleKindListing() const;
  uint64_t GetSaleKindAuction() const;
  std::string GetChunkClaimsAddress() const;
  std::string GetGlobalParamsAddress() const;
  uint64_t GetFeeBps() const;
  uint64_t GetNextListingId() const;
  uint64_t GetNextAuctionId() const;
  ListingState GetListing(const std::string& listingId) const;
  AuctionState GetAuction(const std::string& auctionId) const;
  std::string GetActiveListingByTokenId(const std::string& tokenId) const;
  std::string GetActiveAuctionByTokenId(const std::string& tokenId) const;
  BlockchainTransactionReceipt SetFeeBps(uint64_t newFeeBps) const;
  MarketplaceListingTransaction CreateListing(int32_t x, int32_t y, const std::string& price) const;
  BlockchainTransactionReceipt CancelListing(const std::string& listingId) const;
  BlockchainTransactionReceipt InvalidateStaleListing(const std::string& listingId) const;
  BlockchainTransactionReceipt PurchaseListing(const std::string& listingId, const std::string& paymentValue) const;
  MarketplaceAuctionTransaction CreateAuction(
      int32_t x,
      int32_t y,
      const std::string& reservePrice,
      const std::string& minBidIncrement,
      uint64_t durationSeconds) const;
  BlockchainTransactionReceipt PlaceAuctionBid(const std::string& auctionId, const std::string& bidValue) const;
  BlockchainTransactionReceipt CancelAuction(const std::string& auctionId) const;
  BlockchainTransactionReceipt InvalidateStaleAuction(const std::string& auctionId) const;
  BlockchainTransactionReceipt SettleAuction(const std::string& auctionId) const;
  SaleState GetSaleStateForChunk(int32_t x, int32_t y) const;
  SaleState GetSaleStateForToken(const std::string& tokenId) const;
  BlockchainTransactionReceipt WithdrawFees(const std::string& recipient) const;

 protected:
  const char* GetContractName() const override;
};
