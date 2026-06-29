#include "MarketplaceContract.h"

#include <vector>

static constexpr const char* SELECTOR_FEE_BPS = "0x24a9d853";
static constexpr const char* SELECTOR_GET_SALE_STATE_FOR_CHUNK = "0x6a0551eb";
static constexpr const char* SELECTOR_GET_SALE_STATE_FOR_TOKEN = "0x43fbccfc";

static SaleState DecodeSaleState(const std::string& resultHex)
{
  SaleState state = {};
  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  if (words.size() < 13) return state;

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

uint64_t MarketplaceContract::GetFeeBps() const
{
  std::string resultHex = {};
  if (!Call(BuildBlockchainCallData(SELECTOR_FEE_BPS, {}), &resultHex))
  {
    return 0;
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  return words.empty() ? 0 : DecodeBlockchainWordAsUint64(words[0]);
}

SaleState MarketplaceContract::GetSaleStateForChunk(int32_t x, int32_t y) const
{
  std::string resultHex = {};
  if (!Call(
        BuildBlockchainCallData(
          SELECTOR_GET_SALE_STATE_FOR_CHUNK,
          {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y)}
        ),
        &resultHex
      ))
  {
    return {};
  }

  return DecodeSaleState(resultHex);
}

SaleState MarketplaceContract::GetSaleStateForToken(uint64_t tokenId) const
{
  std::string resultHex = {};
  if (!Call(BuildBlockchainCallData(SELECTOR_GET_SALE_STATE_FOR_TOKEN, {EncodeBlockchainUint64Argument(tokenId)}), &resultHex))
  {
    return {};
  }

  return DecodeSaleState(resultHex);
}
