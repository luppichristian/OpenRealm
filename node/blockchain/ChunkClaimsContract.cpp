#include "ChunkClaimsContract.h"

#include <vector>

static constexpr const char* SIGNATURE_OWNER = "owner()";
static constexpr const char* SELECTOR_OWNER = "0x8da5cb5b";
static constexpr const char* SIGNATURE_TRANSFER_OWNERSHIP = "transferOwnership(address)";
static constexpr const char* SELECTOR_TRANSFER_OWNERSHIP = "0xf2fde38b";
static constexpr const char* SIGNATURE_REGISTRY = "registry()";
static constexpr const char* SELECTOR_REGISTRY = "0x7b103999";
static constexpr const char* SIGNATURE_GLOBAL_PARAMS = "globalParams()";
static constexpr const char* SELECTOR_GLOBAL_PARAMS = "0x3acf597c";
static constexpr const char* SIGNATURE_MARKETPLACE = "marketplace()";
static constexpr const char* SELECTOR_MARKETPLACE = "0xabc8c7af";
static constexpr const char* SIGNATURE_NEXT_TOKEN_ID = "nextTokenId()";
static constexpr const char* SELECTOR_NEXT_TOKEN_ID = "0x75794a3c";
static constexpr const char* SIGNATURE_SET_MARKETPLACE = "SetMarketplace(address)";
static constexpr const char* SELECTOR_SET_MARKETPLACE = "0x53336f5d";
static constexpr const char* SIGNATURE_CLAIM_CHUNK = "ClaimChunk(int32,int32)";
static constexpr const char* SELECTOR_CLAIM_CHUNK = "0x4ac503b0";
static constexpr const char* SIGNATURE_ABANDON_CHUNK = "AbandonChunk(int32,int32)";
static constexpr const char* SELECTOR_ABANDON_CHUNK = "0x1e6060ef";
static constexpr const char* SIGNATURE_TRANSFER_CHUNK = "TransferChunk(int32,int32,address)";
static constexpr const char* SELECTOR_TRANSFER_CHUNK = "0x425ea499";
static constexpr const char* SIGNATURE_SET_CHUNK_EDITOR = "SetChunkEditor(int32,int32,address,bool)";
static constexpr const char* SELECTOR_SET_CHUNK_EDITOR = "0x09864f20";
static constexpr const char* SIGNATURE_MARKETPLACE_TRANSFER_CHUNK = "MarketplaceTransferChunk(int32,int32,address,address)";
static constexpr const char* SELECTOR_MARKETPLACE_TRANSFER_CHUNK = "0x8ea263a7";
static constexpr const char* SIGNATURE_OWNER_OF = "OwnerOf(int32,int32)";
static constexpr const char* SELECTOR_OWNER_OF = "0x3d209568";
static constexpr const char* SIGNATURE_TOKEN_ID_OF_CHUNK = "TokenIdOfChunk(int32,int32)";
static constexpr const char* SELECTOR_TOKEN_ID_OF_CHUNK = "0x440394c4";
static constexpr const char* SIGNATURE_GET_CLAIM = "GetClaim(int32,int32)";
static constexpr const char* SELECTOR_GET_CLAIM = "0xb8fc7a8c";
static constexpr const char* SIGNATURE_GET_CLAIM_BY_TOKEN_ID = "GetClaimByTokenId(uint256)";
static constexpr const char* SELECTOR_GET_CLAIM_BY_TOKEN_ID = "0x7f56cb56";
static constexpr const char* SIGNATURE_CAN_EDIT = "CanEdit(int32,int32,address)";
static constexpr const char* SELECTOR_CAN_EDIT = "0x07f809c4";
static constexpr const char* SIGNATURE_CAN_EDIT_WITH_RUNTIME_SIGNER = "CanEditWithRuntimeSigner(int32,int32,address)";
static constexpr const char* SELECTOR_CAN_EDIT_WITH_RUNTIME_SIGNER = "0x87e364ab";
static constexpr const char* SIGNATURE_EDITOR_EPOCH_OF_CHUNK = "EditorEpochOfChunk(int32,int32)";
static constexpr const char* SELECTOR_EDITOR_EPOCH_OF_CHUNK = "0x23011b9d";
static constexpr const char* SIGNATURE_GET_CHUNK_RUNTIME_STATE = "GetChunkRuntimeState(int32,int32,address)";
static constexpr const char* SELECTOR_GET_CHUNK_RUNTIME_STATE = "0xc2f6381c";
static constexpr const char* SIGNATURE_IS_REGISTERED_PLAYER = "IsRegisteredPlayer(address)";
static constexpr const char* SELECTOR_IS_REGISTERED_PLAYER = "0xf5b1e7b6";
static constexpr const char* SIGNATURE_ENCODE_CHUNK_KEY = "EncodeChunkKey(int32,int32)";
static constexpr const char* SELECTOR_ENCODE_CHUNK_KEY = "0xdd707254";
static constexpr const char* SIGNATURE_NAME = "name()";
static constexpr const char* SELECTOR_NAME = "0x06fdde03";
static constexpr const char* SIGNATURE_SYMBOL = "symbol()";
static constexpr const char* SELECTOR_SYMBOL = "0x95d89b41";
static constexpr const char* SIGNATURE_BALANCE_OF = "balanceOf(address)";
static constexpr const char* SELECTOR_BALANCE_OF = "0x70a08231";
static constexpr const char* SIGNATURE_OWNER_OF_TOKEN = "ownerOfToken(uint256)";
static constexpr const char* SELECTOR_OWNER_OF_TOKEN = "0xc57ac5d6";
static constexpr const char* SIGNATURE_GET_APPROVED = "getApproved(uint256)";
static constexpr const char* SELECTOR_GET_APPROVED = "0x081812fc";
static constexpr const char* SIGNATURE_IS_APPROVED_FOR_ALL = "isApprovedForAll(address,address)";
static constexpr const char* SELECTOR_IS_APPROVED_FOR_ALL = "0xe985e9c5";
static constexpr const char* SIGNATURE_APPROVE = "approve(address,uint256)";
static constexpr const char* SELECTOR_APPROVE = "0x095ea7b3";
static constexpr const char* SIGNATURE_SET_APPROVAL_FOR_ALL = "setApprovalForAll(address,bool)";
static constexpr const char* SELECTOR_SET_APPROVAL_FOR_ALL = "0xa22cb465";
static constexpr const char* SIGNATURE_TRANSFER_FROM = "transferFrom(address,address,uint256)";
static constexpr const char* SELECTOR_TRANSFER_FROM = "0x23b872dd";
static constexpr const char* SIGNATURE_TOKEN_URI = "tokenURI(uint256)";
static constexpr const char* SELECTOR_TOKEN_URI = "0xc87b56dd";

static std::string DecodeStringResult(const std::vector<std::string>& words)
{
  return words.empty() ? std::string{} : DecodeBlockchainStringAt(words, 0);
}

static ChunkClaimState DecodeChunkClaim(const std::vector<std::string>& words)
{
  ChunkClaimState claim = {};
  if (words.size() < 4)
  {
    return claim;
  }

  claim.available = true;
  claim.tokenId = DecodeBlockchainWordAsUintHex(words[0]);
  claim.x = DecodeBlockchainWordAsInt32(words[1]);
  claim.y = DecodeBlockchainWordAsInt32(words[2]);
  claim.claimedAt = DecodeBlockchainWordAsUint64(words[3]);
  return claim;
}

const char* ChunkClaimsContract::GetContractName() const
{
  return "ChunkClaims";
}

std::string ChunkClaimsContract::GetOwner() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_OWNER, {}), &words) || words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsAddress(words[0]);
}

BlockchainTransactionReceipt ChunkClaimsContract::TransferOwnership(const std::string& newOwner) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(BuildBlockchainCallData(SELECTOR_TRANSFER_OWNERSHIP, {EncodeBlockchainAddressArgument(newOwner)}), &receipt);
  return receipt;
}

std::string ChunkClaimsContract::GetRegistryAddress() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_REGISTRY, {}), &words) || words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsAddress(words[0]);
}

std::string ChunkClaimsContract::GetGlobalParamsAddress() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_GLOBAL_PARAMS, {}), &words) || words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsAddress(words[0]);
}

std::string ChunkClaimsContract::GetMarketplaceAddress() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_MARKETPLACE, {}), &words) || words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsAddress(words[0]);
}

uint64_t ChunkClaimsContract::GetNextTokenId() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_NEXT_TOKEN_ID, {}), &words) || words.empty())
  {
    return 0;
  }
  return DecodeBlockchainWordAsUint64(words[0]);
}

BlockchainTransactionReceipt ChunkClaimsContract::SetMarketplace(const std::string& marketplaceAddress) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(BuildBlockchainCallData(SELECTOR_SET_MARKETPLACE, {EncodeBlockchainAddressArgument(marketplaceAddress)}), &receipt);
  return receipt;
}

ChunkClaimTransaction ChunkClaimsContract::ClaimChunk(int32_t x, int32_t y, const std::string& paymentValue) const
{
  ChunkClaimTransaction result = {};
  const std::string callData = BuildBlockchainCallData(
      SELECTOR_CLAIM_CHUNK,
      {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y)});

  std::vector<std::string> previewWords = {};
  if (CallWords(callData, &previewWords, GetWalletTransactionSender(), paymentValue) && !previewWords.empty())
  {
    result.tokenId = DecodeBlockchainWordAsUintHex(previewWords[0]);
  }

  SendTransaction(callData, &result.receipt, paymentValue);
  return result;
}

BlockchainTransactionReceipt ChunkClaimsContract::AbandonChunk(int32_t x, int32_t y) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(
          SELECTOR_ABANDON_CHUNK,
          {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y)}),
      &receipt);
  return receipt;
}

BlockchainTransactionReceipt ChunkClaimsContract::TransferChunk(int32_t x, int32_t y, const std::string& newOwner) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(
          SELECTOR_TRANSFER_CHUNK,
          {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y), EncodeBlockchainAddressArgument(newOwner)}),
      &receipt);
  return receipt;
}

BlockchainTransactionReceipt ChunkClaimsContract::SetChunkEditor(int32_t x, int32_t y, const std::string& editor, bool allowed) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(
          SELECTOR_SET_CHUNK_EDITOR,
          {
              EncodeBlockchainInt32Argument(x),
              EncodeBlockchainInt32Argument(y),
              EncodeBlockchainAddressArgument(editor),
              EncodeBlockchainBoolArgument(allowed),
          }),
      &receipt);
  return receipt;
}

BlockchainTransactionReceipt ChunkClaimsContract::MarketplaceTransferChunk(
    int32_t x,
    int32_t y,
    const std::string& from,
    const std::string& to
) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(
          SELECTOR_MARKETPLACE_TRANSFER_CHUNK,
          {
              EncodeBlockchainInt32Argument(x),
              EncodeBlockchainInt32Argument(y),
              EncodeBlockchainAddressArgument(from),
              EncodeBlockchainAddressArgument(to),
          }),
      &receipt);
  return receipt;
}

std::string ChunkClaimsContract::OwnerOf(int32_t x, int32_t y) const
{
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(SELECTOR_OWNER_OF, {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y)}),
          &words) ||
      words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsAddress(words[0]);
}

std::string ChunkClaimsContract::TokenIdOfChunk(int32_t x, int32_t y) const
{
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(
              SELECTOR_TOKEN_ID_OF_CHUNK,
              {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y)}),
          &words) ||
      words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsUintHex(words[0]);
}

ChunkClaimState ChunkClaimsContract::GetClaim(int32_t x, int32_t y) const
{
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(SELECTOR_GET_CLAIM, {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y)}),
          &words))
  {
    return {};
  }
  return DecodeChunkClaim(words);
}

ChunkClaimState ChunkClaimsContract::GetClaimByTokenId(const std::string& tokenId) const
{
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(SELECTOR_GET_CLAIM_BY_TOKEN_ID, {EncodeBlockchainUint256Argument(tokenId)}),
          &words))
  {
    return {};
  }
  return DecodeChunkClaim(words);
}

bool ChunkClaimsContract::CanEdit(int32_t x, int32_t y, const std::string& account) const
{
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(
              SELECTOR_CAN_EDIT,
              {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y), EncodeBlockchainAddressArgument(account)}),
          &words) ||
      words.empty())
  {
    return false;
  }
  return DecodeBlockchainWordAsBool(words[0]);
}

ChunkEditState ChunkClaimsContract::CanEditWithRuntimeSigner(int32_t x, int32_t y, const std::string& actor) const
{
  ChunkEditState state = {};
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(
              SELECTOR_CAN_EDIT_WITH_RUNTIME_SIGNER,
              {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y), EncodeBlockchainAddressArgument(actor)}),
          &words) ||
      words.size() < 3)
  {
    return state;
  }

  state.available = true;
  state.allowed = DecodeBlockchainWordAsBool(words[0]);
  state.resolvedActor = DecodeBlockchainWordAsAddress(words[1]);
  state.actorUsesRuntimeSession = DecodeBlockchainWordAsBool(words[2]);
  return state;
}

uint64_t ChunkClaimsContract::EditorEpochOfChunk(int32_t x, int32_t y) const
{
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(
              SELECTOR_EDITOR_EPOCH_OF_CHUNK,
              {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y)}),
          &words) ||
      words.empty())
  {
    return 0;
  }
  return DecodeBlockchainWordAsUint64(words[0]);
}

ChunkRuntimeState ChunkClaimsContract::GetChunkRuntimeState(int32_t x, int32_t y, const std::string& actor) const
{
  ChunkRuntimeState state = {};
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(
              SELECTOR_GET_CHUNK_RUNTIME_STATE,
              {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y), EncodeBlockchainAddressArgument(actor)}),
          &words) ||
      words.size() < 11)
  {
    return state;
  }

  state.available = true;
  state.claimed = DecodeBlockchainWordAsBool(words[0]);
  state.tokenId = DecodeBlockchainWordAsUintHex(words[1]);
  state.x = DecodeBlockchainWordAsInt32(words[2]);
  state.y = DecodeBlockchainWordAsInt32(words[3]);
  state.owner = DecodeBlockchainWordAsAddress(words[4]);
  state.ownerPlayerId = DecodeBlockchainWordAsUintHex(words[5]);
  state.claimedAt = DecodeBlockchainWordAsUint64(words[6]);
  state.editorEpoch = DecodeBlockchainWordAsUint64(words[7]);
  state.resolvedActor = DecodeBlockchainWordAsAddress(words[8]);
  state.actorRegistered = DecodeBlockchainWordAsBool(words[9]);
  state.actorCanEdit = DecodeBlockchainWordAsBool(words[10]);
  if (words.size() > 11)
  {
    state.actorUsesRuntimeSession = DecodeBlockchainWordAsBool(words[11]);
  }
  return state;
}

bool ChunkClaimsContract::IsRegisteredPlayer(const std::string& account) const
{
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(SELECTOR_IS_REGISTERED_PLAYER, {EncodeBlockchainAddressArgument(account)}),
          &words) ||
      words.empty())
  {
    return false;
  }
  return DecodeBlockchainWordAsBool(words[0]);
}

std::string ChunkClaimsContract::EncodeChunkKey(int32_t x, int32_t y) const
{
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(
              SELECTOR_ENCODE_CHUNK_KEY,
              {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y)}),
          &words) ||
      words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsBytes32Hex(words[0]);
}

std::string ChunkClaimsContract::GetName() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_NAME, {}), &words))
  {
    return {};
  }
  return DecodeStringResult(words);
}

std::string ChunkClaimsContract::GetSymbol() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_SYMBOL, {}), &words))
  {
    return {};
  }
  return DecodeStringResult(words);
}

std::string ChunkClaimsContract::BalanceOf(const std::string& owner) const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_BALANCE_OF, {EncodeBlockchainAddressArgument(owner)}), &words) ||
      words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsUintHex(words[0]);
}

std::string ChunkClaimsContract::OwnerOfToken(const std::string& tokenId) const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_OWNER_OF_TOKEN, {EncodeBlockchainUint256Argument(tokenId)}), &words) ||
      words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsAddress(words[0]);
}

std::string ChunkClaimsContract::GetApproved(const std::string& tokenId) const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_GET_APPROVED, {EncodeBlockchainUint256Argument(tokenId)}), &words) ||
      words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsAddress(words[0]);
}

bool ChunkClaimsContract::IsApprovedForAll(const std::string& owner, const std::string& operatorAddress) const
{
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(
              SELECTOR_IS_APPROVED_FOR_ALL,
              {EncodeBlockchainAddressArgument(owner), EncodeBlockchainAddressArgument(operatorAddress)}),
          &words) ||
      words.empty())
  {
    return false;
  }
  return DecodeBlockchainWordAsBool(words[0]);
}

BlockchainTransactionReceipt ChunkClaimsContract::Approve(const std::string& to, const std::string& tokenId) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(
          SELECTOR_APPROVE,
          {EncodeBlockchainAddressArgument(to), EncodeBlockchainUint256Argument(tokenId)}),
      &receipt);
  return receipt;
}

BlockchainTransactionReceipt ChunkClaimsContract::SetApprovalForAll(const std::string& operatorAddress, bool approved) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(
          SELECTOR_SET_APPROVAL_FOR_ALL,
          {EncodeBlockchainAddressArgument(operatorAddress), EncodeBlockchainBoolArgument(approved)}),
      &receipt);
  return receipt;
}

BlockchainTransactionReceipt ChunkClaimsContract::TransferFrom(
    const std::string& from,
    const std::string& to,
    const std::string& tokenId
) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(
          SELECTOR_TRANSFER_FROM,
          {
              EncodeBlockchainAddressArgument(from),
              EncodeBlockchainAddressArgument(to),
              EncodeBlockchainUint256Argument(tokenId),
          }),
      &receipt);
  return receipt;
}

std::string ChunkClaimsContract::TokenURI(const std::string& tokenId) const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_TOKEN_URI, {EncodeBlockchainUint256Argument(tokenId)}), &words))
  {
    return {};
  }
  return DecodeStringResult(words);
}
