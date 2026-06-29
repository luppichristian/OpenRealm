#include "GlobalParamsContract.h"

#include <vector>

static constexpr const char* SELECTOR_MIN_CHUNK_COORD = "0x57da7eaa";
static constexpr const char* SELECTOR_MAX_CHUNK_COORD = "0xcef63fb2";
static constexpr const char* SELECTOR_MIN_CHUNK_PRICE = "0xae1664c6";
static constexpr const char* SELECTOR_MAX_FEE_BPS = "0xd55be8c6";
static constexpr const char* SELECTOR_MIN_AUCTION_DURATION = "0xc2f50a7a";

const char* GlobalParamsContract::GetContractName() const
{
  return "GlobalParams";
}

GlobalParamsState GlobalParamsContract::GetState() const
{
  GlobalParamsState state = {};

  std::string minChunkCoordHex = {};
  std::string maxChunkCoordHex = {};
  std::string minChunkPriceHex = {};
  std::string maxFeeBpsHex = {};
  std::string minAuctionDurationHex = {};
  if (!Call(BuildBlockchainCallData(SELECTOR_MIN_CHUNK_COORD, {}), &minChunkCoordHex)) return state;
  if (!Call(BuildBlockchainCallData(SELECTOR_MAX_CHUNK_COORD, {}), &maxChunkCoordHex)) return state;
  if (!Call(BuildBlockchainCallData(SELECTOR_MIN_CHUNK_PRICE, {}), &minChunkPriceHex)) return state;
  if (!Call(BuildBlockchainCallData(SELECTOR_MAX_FEE_BPS, {}), &maxFeeBpsHex)) return state;
  if (!Call(BuildBlockchainCallData(SELECTOR_MIN_AUCTION_DURATION, {}), &minAuctionDurationHex)) return state;

  std::vector<std::string> minChunkCoordWords = SplitBlockchainWords(minChunkCoordHex);
  std::vector<std::string> maxChunkCoordWords = SplitBlockchainWords(maxChunkCoordHex);
  std::vector<std::string> minChunkPriceWords = SplitBlockchainWords(minChunkPriceHex);
  std::vector<std::string> maxFeeBpsWords = SplitBlockchainWords(maxFeeBpsHex);
  std::vector<std::string> minAuctionDurationWords = SplitBlockchainWords(minAuctionDurationHex);
  if (minChunkCoordWords.empty() || maxChunkCoordWords.empty() || minChunkPriceWords.empty() || maxFeeBpsWords.empty() || minAuctionDurationWords.empty()) return state;

  state.available = true;
  state.minChunkCoord = DecodeBlockchainWordAsInt32(minChunkCoordWords[0]);
  state.maxChunkCoord = DecodeBlockchainWordAsInt32(maxChunkCoordWords[0]);
  state.minChunkPrice = DecodeBlockchainWordAsUintHex(minChunkPriceWords[0]);
  state.maxFeeBps = DecodeBlockchainWordAsUint64(maxFeeBpsWords[0]);
  state.minAuctionDuration = DecodeBlockchainWordAsUint64(minAuctionDurationWords[0]);
  return state;
}
