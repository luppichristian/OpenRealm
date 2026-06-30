#include "GlobalParamsContract.h"

#include <vector>

static constexpr const char* SIGNATURE_MIN_CHUNK_COORD = "MIN_CHUNK_COORD()";
static constexpr const char* SELECTOR_MIN_CHUNK_COORD = "0x57da7eaa";
static constexpr const char* SIGNATURE_MAX_CHUNK_COORD = "MAX_CHUNK_COORD()";
static constexpr const char* SELECTOR_MAX_CHUNK_COORD = "0xcef63fb2";
static constexpr const char* SIGNATURE_MIN_CHUNK_PRICE = "MIN_CHUNK_PRICE()";
static constexpr const char* SELECTOR_MIN_CHUNK_PRICE = "0xae1664c6";
static constexpr const char* SIGNATURE_MAX_FEE_BPS = "MAX_FEE_BPS()";
static constexpr const char* SELECTOR_MAX_FEE_BPS = "0xd55be8c6";
static constexpr const char* SIGNATURE_MIN_AUCTION_DURATION = "MIN_AUCTION_DURATION()";
static constexpr const char* SELECTOR_MIN_AUCTION_DURATION = "0xc2f50a7a";

const char* GlobalParamsContract::GetContractName() const
{
  return "GlobalParams";
}

int32_t GlobalParamsContract::GetMinChunkCoord() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_MIN_CHUNK_COORD, {}), &words) || words.empty())
  {
    return 0;
  }
  return DecodeBlockchainWordAsInt32(words[0]);
}

int32_t GlobalParamsContract::GetMaxChunkCoord() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_MAX_CHUNK_COORD, {}), &words) || words.empty())
  {
    return 0;
  }
  return DecodeBlockchainWordAsInt32(words[0]);
}

std::string GlobalParamsContract::GetMinChunkPrice() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_MIN_CHUNK_PRICE, {}), &words) || words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsUintHex(words[0]);
}

uint64_t GlobalParamsContract::GetMaxFeeBps() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_MAX_FEE_BPS, {}), &words) || words.empty())
  {
    return 0;
  }
  return DecodeBlockchainWordAsUint64(words[0]);
}

uint64_t GlobalParamsContract::GetMinAuctionDuration() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_MIN_AUCTION_DURATION, {}), &words) || words.empty())
  {
    return 0;
  }
  return DecodeBlockchainWordAsUint64(words[0]);
}

GlobalParamsState GlobalParamsContract::GetState() const
{
  GlobalParamsState state = {};
  state.minChunkCoord = GetMinChunkCoord();
  state.maxChunkCoord = GetMaxChunkCoord();
  state.minChunkPrice = GetMinChunkPrice();
  state.maxFeeBps = GetMaxFeeBps();
  state.minAuctionDuration = GetMinAuctionDuration();
  state.available = !state.minChunkPrice.empty();
  return state;
}
