#include "MarketplaceContract.h"

#include <vector>

static constexpr const char* SIGNATURE_OWNER = "owner()";
static constexpr const char* SELECTOR_OWNER = "0x8da5cb5b";
static constexpr const char* SIGNATURE_TRANSFER_OWNERSHIP = "transferOwnership(address)";
static constexpr const char* SELECTOR_TRANSFER_OWNERSHIP = "0xf2fde38b";
static constexpr const char* SIGNATURE_SALE_KIND_NONE = "SALE_KIND_NONE()";
static constexpr const char* SELECTOR_SALE_KIND_NONE = "0x83c147f3";
static constexpr const char* SIGNATURE_SALE_KIND_LISTING = "SALE_KIND_LISTING()";
static constexpr const char* SELECTOR_SALE_KIND_LISTING = "0x7fa454c2";
static constexpr const char* SIGNATURE_SALE_KIND_AUCTION = "SALE_KIND_AUCTION()";
static constexpr const char* SELECTOR_SALE_KIND_AUCTION = "0x60a0ab83";
static constexpr const char* SIGNATURE_CHUNK_CLAIMS = "chunkClaims()";
static constexpr const char* SELECTOR_CHUNK_CLAIMS = "0x08ec4b91";
static constexpr const char* SIGNATURE_GLOBAL_PARAMS = "globalParams()";
static constexpr const char* SELECTOR_GLOBAL_PARAMS = "0x3acf597c";
static constexpr const char* SIGNATURE_FEE_BPS = "feeBps()";
static constexpr const char* SELECTOR_FEE_BPS = "0x24a9d853";
static constexpr const char* SIGNATURE_NEXT_LISTING_ID = "nextListingId()";
static constexpr const char* SELECTOR_NEXT_LISTING_ID = "0xaaccf1ec";
static constexpr const char* SIGNATURE_NEXT_AUCTION_ID = "nextAuctionId()";
static constexpr const char* SELECTOR_NEXT_AUCTION_ID = "0xfc528482";
static constexpr const char* SIGNATURE_LISTINGS = "listings(uint256)";
static constexpr const char* SELECTOR_LISTINGS = "0xde74e57b";
static constexpr const char* SIGNATURE_AUCTIONS = "auctions(uint256)";
static constexpr const char* SELECTOR_AUCTIONS = "0x571a26a0";
static constexpr const char* SIGNATURE_ACTIVE_LISTING_BY_TOKEN_ID = "activeListingByTokenId(uint256)";
static constexpr const char* SELECTOR_ACTIVE_LISTING_BY_TOKEN_ID = "0x6cb0577f";
static constexpr const char* SIGNATURE_ACTIVE_AUCTION_BY_TOKEN_ID = "activeAuctionByTokenId(uint256)";
static constexpr const char* SELECTOR_ACTIVE_AUCTION_BY_TOKEN_ID = "0xffed6016";
static constexpr const char* SIGNATURE_SET_FEE_BPS = "SetFeeBps(uint96)";
static constexpr const char* SELECTOR_SET_FEE_BPS = "0x5ae401c7";
static constexpr const char* SIGNATURE_CREATE_LISTING = "CreateListing(int32,int32,uint256)";
static constexpr const char* SELECTOR_CREATE_LISTING = "0xc269d4af";
static constexpr const char* SIGNATURE_CANCEL_LISTING = "CancelListing(uint256)";
static constexpr const char* SELECTOR_CANCEL_LISTING = "0xa5a50019";
static constexpr const char* SIGNATURE_INVALIDATE_STALE_LISTING = "InvalidateStaleListing(uint256)";
static constexpr const char* SELECTOR_INVALIDATE_STALE_LISTING = "0xd7f86764";
static constexpr const char* SIGNATURE_PURCHASE_LISTING = "PurchaseListing(uint256)";
static constexpr const char* SELECTOR_PURCHASE_LISTING = "0x400a1fc7";
static constexpr const char* SIGNATURE_CREATE_AUCTION = "CreateAuction(int32,int32,uint256,uint256,uint64)";
static constexpr const char* SELECTOR_CREATE_AUCTION = "0xdcb7fc3c";
static constexpr const char* SIGNATURE_PLACE_AUCTION_BID = "PlaceAuctionBid(uint256)";
static constexpr const char* SELECTOR_PLACE_AUCTION_BID = "0x2c3a46e7";
static constexpr const char* SIGNATURE_CANCEL_AUCTION = "CancelAuction(uint256)";
static constexpr const char* SELECTOR_CANCEL_AUCTION = "0xbea0e66c";
static constexpr const char* SIGNATURE_INVALIDATE_STALE_AUCTION = "InvalidateStaleAuction(uint256)";
static constexpr const char* SELECTOR_INVALIDATE_STALE_AUCTION = "0x30ef37f2";
static constexpr const char* SIGNATURE_SETTLE_AUCTION = "SettleAuction(uint256)";
static constexpr const char* SELECTOR_SETTLE_AUCTION = "0x03af424b";
static constexpr const char* SIGNATURE_GET_SALE_STATE_FOR_CHUNK = "GetSaleStateForChunk(int32,int32)";
static constexpr const char* SELECTOR_GET_SALE_STATE_FOR_CHUNK = "0x6a0551eb";
static constexpr const char* SIGNATURE_GET_SALE_STATE_FOR_TOKEN = "GetSaleStateForToken(uint256)";
static constexpr const char* SELECTOR_GET_SALE_STATE_FOR_TOKEN = "0x43fbccfc";
static constexpr const char* SIGNATURE_WITHDRAW_FEES = "WithdrawFees(address)";
static constexpr const char* SELECTOR_WITHDRAW_FEES = "0xd3719f04";

static ListingState DecodeListing(const std::vector<std::string>& words)
{
  ListingState state = {};
  if (words.size() < 7)
  {
    return state;
  }

  state.available = true;
  state.id = DecodeBlockchainWordAsUintHex(words[0]);
  state.tokenId = DecodeBlockchainWordAsUintHex(words[1]);
  state.x = DecodeBlockchainWordAsInt32(words[2]);
  state.y = DecodeBlockchainWordAsInt32(words[3]);
  state.seller = DecodeBlockchainWordAsAddress(words[4]);
  state.price = DecodeBlockchainWordAsUintHex(words[5]);
  state.active = DecodeBlockchainWordAsBool(words[6]);
  return state;
}

static AuctionState DecodeAuction(const std::vector<std::string>& words)
{
  AuctionState state = {};
  if (words.size() < 11)
  {
    return state;
  }

  state.available = true;
  state.id = DecodeBlockchainWordAsUintHex(words[0]);
  state.tokenId = DecodeBlockchainWordAsUintHex(words[1]);
  state.x = DecodeBlockchainWordAsInt32(words[2]);
  state.y = DecodeBlockchainWordAsInt32(words[3]);
  state.seller = DecodeBlockchainWordAsAddress(words[4]);
  state.reservePrice = DecodeBlockchainWordAsUintHex(words[5]);
  state.minBidIncrement = DecodeBlockchainWordAsUintHex(words[6]);
  state.endTime = DecodeBlockchainWordAsUint64(words[7]);
  state.highestBidder = DecodeBlockchainWordAsAddress(words[8]);
  state.highestBid = DecodeBlockchainWordAsUintHex(words[9]);
  state.active = DecodeBlockchainWordAsBool(words[10]);
  if (words.size() > 11)
  {
    state.settled = DecodeBlockchainWordAsBool(words[11]);
  }
  return state;
}

static SaleState DecodeSaleState(const std::vector<std::string>& words)
{
  SaleState state = {};
  if (words.size() < 13)
  {
    return state;
  }

  state.available = true;
  state.saleKind = (uint8_t)DecodeBlockchainWordAsUint64(words[0]);
  state.saleId = DecodeBlockchainWordAsUintHex(words[1]);
  state.tokenId = DecodeBlockchainWordAsUintHex(words[2]);
  state.x = DecodeBlockchainWordAsInt32(words[3]);
  state.y = DecodeBlockchainWordAsInt32(words[4]);
  state.seller = DecodeBlockchainWordAsAddress(words[5]);
  state.sellerStillOwnsChunk = DecodeBlockchainWordAsBool(words[6]);
  state.active = DecodeBlockchainWordAsBool(words[7]);
  state.price = DecodeBlockchainWordAsUintHex(words[8]);
  state.reservePrice = DecodeBlockchainWordAsUintHex(words[9]);
  state.minBidIncrement = DecodeBlockchainWordAsUintHex(words[10]);
  state.endTime = DecodeBlockchainWordAsUint64(words[11]);
  state.highestBidder = DecodeBlockchainWordAsAddress(words[12]);
  if (words.size() > 13)
  {
    state.highestBid = DecodeBlockchainWordAsUintHex(words[13]);
  }
  return state;
}

const char* MarketplaceContract::GetContractName() const
{
  return "Marketplace";
}

std::string MarketplaceContract::GetOwner() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_OWNER, {}), &words) || words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsAddress(words[0]);
}

BlockchainTransactionReceipt MarketplaceContract::TransferOwnership(const std::string& newOwner) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(BuildBlockchainCallData(SELECTOR_TRANSFER_OWNERSHIP, {EncodeBlockchainAddressArgument(newOwner)}), &receipt);
  return receipt;
}

uint64_t MarketplaceContract::GetSaleKindNone() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_SALE_KIND_NONE, {}), &words) || words.empty())
  {
    return 0;
  }
  return DecodeBlockchainWordAsUint64(words[0]);
}

uint64_t MarketplaceContract::GetSaleKindListing() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_SALE_KIND_LISTING, {}), &words) || words.empty())
  {
    return 0;
  }
  return DecodeBlockchainWordAsUint64(words[0]);
}

uint64_t MarketplaceContract::GetSaleKindAuction() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_SALE_KIND_AUCTION, {}), &words) || words.empty())
  {
    return 0;
  }
  return DecodeBlockchainWordAsUint64(words[0]);
}

std::string MarketplaceContract::GetChunkClaimsAddress() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_CHUNK_CLAIMS, {}), &words) || words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsAddress(words[0]);
}

std::string MarketplaceContract::GetGlobalParamsAddress() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_GLOBAL_PARAMS, {}), &words) || words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsAddress(words[0]);
}

uint64_t MarketplaceContract::GetFeeBps() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_FEE_BPS, {}), &words) || words.empty())
  {
    return 0;
  }
  return DecodeBlockchainWordAsUint64(words[0]);
}

uint64_t MarketplaceContract::GetNextListingId() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_NEXT_LISTING_ID, {}), &words) || words.empty())
  {
    return 0;
  }
  return DecodeBlockchainWordAsUint64(words[0]);
}

uint64_t MarketplaceContract::GetNextAuctionId() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_NEXT_AUCTION_ID, {}), &words) || words.empty())
  {
    return 0;
  }
  return DecodeBlockchainWordAsUint64(words[0]);
}

ListingState MarketplaceContract::GetListing(const std::string& listingId) const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_LISTINGS, {EncodeBlockchainUint256Argument(listingId)}), &words))
  {
    return {};
  }
  return DecodeListing(words);
}

AuctionState MarketplaceContract::GetAuction(const std::string& auctionId) const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_AUCTIONS, {EncodeBlockchainUint256Argument(auctionId)}), &words))
  {
    return {};
  }
  return DecodeAuction(words);
}

std::string MarketplaceContract::GetActiveListingByTokenId(const std::string& tokenId) const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_ACTIVE_LISTING_BY_TOKEN_ID, {EncodeBlockchainUint256Argument(tokenId)}), &words) ||
      words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsUintHex(words[0]);
}

std::string MarketplaceContract::GetActiveAuctionByTokenId(const std::string& tokenId) const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_ACTIVE_AUCTION_BY_TOKEN_ID, {EncodeBlockchainUint256Argument(tokenId)}), &words) ||
      words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsUintHex(words[0]);
}

BlockchainTransactionReceipt MarketplaceContract::SetFeeBps(uint64_t newFeeBps) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(BuildBlockchainCallData(SELECTOR_SET_FEE_BPS, {EncodeBlockchainUint96Argument(newFeeBps)}), &receipt);
  return receipt;
}

MarketplaceListingTransaction MarketplaceContract::CreateListing(int32_t x, int32_t y, const std::string& price) const
{
  MarketplaceListingTransaction result = {};
  const std::string callData = BuildBlockchainCallData(
      SELECTOR_CREATE_LISTING,
      {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y), EncodeBlockchainUint256Argument(price)});

  std::vector<std::string> previewWords = {};
  if (CallWords(callData, &previewWords, GetWalletTransactionSender()) && !previewWords.empty())
  {
    result.listingId = DecodeBlockchainWordAsUintHex(previewWords[0]);
  }

  SendTransaction(callData, &result.receipt);
  return result;
}

BlockchainTransactionReceipt MarketplaceContract::CancelListing(const std::string& listingId) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(BuildBlockchainCallData(SELECTOR_CANCEL_LISTING, {EncodeBlockchainUint256Argument(listingId)}), &receipt);
  return receipt;
}

BlockchainTransactionReceipt MarketplaceContract::InvalidateStaleListing(const std::string& listingId) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(BuildBlockchainCallData(SELECTOR_INVALIDATE_STALE_LISTING, {EncodeBlockchainUint256Argument(listingId)}), &receipt);
  return receipt;
}

BlockchainTransactionReceipt MarketplaceContract::PurchaseListing(const std::string& listingId, const std::string& paymentValue) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(SELECTOR_PURCHASE_LISTING, {EncodeBlockchainUint256Argument(listingId)}),
      &receipt,
      paymentValue);
  return receipt;
}

MarketplaceAuctionTransaction MarketplaceContract::CreateAuction(
    int32_t x,
    int32_t y,
    const std::string& reservePrice,
    const std::string& minBidIncrement,
    uint64_t durationSeconds
) const
{
  MarketplaceAuctionTransaction result = {};
  const std::string callData = BuildBlockchainCallData(
      SELECTOR_CREATE_AUCTION,
      {
          EncodeBlockchainInt32Argument(x),
          EncodeBlockchainInt32Argument(y),
          EncodeBlockchainUint256Argument(reservePrice),
          EncodeBlockchainUint256Argument(minBidIncrement),
          EncodeBlockchainUint64Argument(durationSeconds),
      });

  std::vector<std::string> previewWords = {};
  if (CallWords(callData, &previewWords, GetWalletTransactionSender()) && !previewWords.empty())
  {
    result.auctionId = DecodeBlockchainWordAsUintHex(previewWords[0]);
  }

  SendTransaction(callData, &result.receipt);
  return result;
}

BlockchainTransactionReceipt MarketplaceContract::PlaceAuctionBid(const std::string& auctionId, const std::string& bidValue) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(SELECTOR_PLACE_AUCTION_BID, {EncodeBlockchainUint256Argument(auctionId)}),
      &receipt,
      bidValue);
  return receipt;
}

BlockchainTransactionReceipt MarketplaceContract::CancelAuction(const std::string& auctionId) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(BuildBlockchainCallData(SELECTOR_CANCEL_AUCTION, {EncodeBlockchainUint256Argument(auctionId)}), &receipt);
  return receipt;
}

BlockchainTransactionReceipt MarketplaceContract::InvalidateStaleAuction(const std::string& auctionId) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(BuildBlockchainCallData(SELECTOR_INVALIDATE_STALE_AUCTION, {EncodeBlockchainUint256Argument(auctionId)}), &receipt);
  return receipt;
}

BlockchainTransactionReceipt MarketplaceContract::SettleAuction(const std::string& auctionId) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(BuildBlockchainCallData(SELECTOR_SETTLE_AUCTION, {EncodeBlockchainUint256Argument(auctionId)}), &receipt);
  return receipt;
}

SaleState MarketplaceContract::GetSaleStateForChunk(int32_t x, int32_t y) const
{
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(
              SELECTOR_GET_SALE_STATE_FOR_CHUNK,
              {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y)}),
          &words))
  {
    return {};
  }
  return DecodeSaleState(words);
}

SaleState MarketplaceContract::GetSaleStateForToken(const std::string& tokenId) const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_GET_SALE_STATE_FOR_TOKEN, {EncodeBlockchainUint256Argument(tokenId)}), &words))
  {
    return {};
  }
  return DecodeSaleState(words);
}

BlockchainTransactionReceipt MarketplaceContract::WithdrawFees(const std::string& recipient) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(BuildBlockchainCallData(SELECTOR_WITHDRAW_FEES, {EncodeBlockchainAddressArgument(recipient)}), &receipt);
  return receipt;
}
