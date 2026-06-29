#include "ChunkClaimsContract.h"

#include <vector>

static constexpr const char* SELECTOR_OWNER_OF = "0x3d209568";
static constexpr const char* SELECTOR_TOKEN_ID_OF_CHUNK = "0x440394c4";
static constexpr const char* SELECTOR_GET_CLAIM = "0xb8fc7a8c";
static constexpr const char* SELECTOR_GET_CLAIM_BY_TOKEN_ID = "0x7f56cb56";
static constexpr const char* SELECTOR_CAN_EDIT = "0x07f809c4";
static constexpr const char* SELECTOR_CAN_EDIT_WITH_RUNTIME_SIGNER = "0x87e364ab";
static constexpr const char* SELECTOR_EDITOR_EPOCH_OF_CHUNK = "0x23011b9d";
static constexpr const char* SELECTOR_GET_CHUNK_RUNTIME_STATE = "0xc2f6381c";
static constexpr const char* SELECTOR_IS_REGISTERED_PLAYER = "0xf5b1e7b6";

const char* ChunkClaimsContract::GetContractName() const
{
  return "ChunkClaims";
}

std::string ChunkClaimsContract::OwnerOf(int32_t x, int32_t y) const
{
  std::string resultHex = {};
  if (!Call(
        BuildBlockchainCallData(
          SELECTOR_OWNER_OF,
          {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y)}
        ),
        &resultHex
      ))
  {
    return {};
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  return words.empty() ? std::string{} : DecodeBlockchainWordAsAddress(words[0]);
}

std::string ChunkClaimsContract::TokenIdOfChunk(int32_t x, int32_t y) const
{
  std::string resultHex = {};
  if (!Call(
        BuildBlockchainCallData(
          SELECTOR_TOKEN_ID_OF_CHUNK,
          {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y)}
        ),
        &resultHex
      ))
  {
    return {};
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  return words.empty() ? std::string{} : DecodeBlockchainWordAsUintHex(words[0]);
}

ChunkClaimState ChunkClaimsContract::GetClaim(int32_t x, int32_t y) const
{
  ChunkClaimState claim = {};

  std::string resultHex = {};
  if (!Call(
        BuildBlockchainCallData(
          SELECTOR_GET_CLAIM,
          {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y)}
        ),
        &resultHex
      ))
  {
    return claim;
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  if (words.size() < 4) return claim;

  claim.available = true;
  claim.tokenId = DecodeBlockchainWordAsUintHex(words[0]);
  claim.x = DecodeBlockchainWordAsInt32(words[1]);
  claim.y = DecodeBlockchainWordAsInt32(words[2]);
  claim.claimedAt = DecodeBlockchainWordAsUint64(words[3]);
  return claim;
}

ChunkClaimState ChunkClaimsContract::GetClaimByTokenId(uint64_t tokenId) const
{
  ChunkClaimState claim = {};

  std::string resultHex = {};
  if (!Call(BuildBlockchainCallData(SELECTOR_GET_CLAIM_BY_TOKEN_ID, {EncodeBlockchainUint64Argument(tokenId)}), &resultHex))
  {
    return claim;
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  if (words.size() < 4) return claim;

  claim.available = true;
  claim.tokenId = DecodeBlockchainWordAsUintHex(words[0]);
  claim.x = DecodeBlockchainWordAsInt32(words[1]);
  claim.y = DecodeBlockchainWordAsInt32(words[2]);
  claim.claimedAt = DecodeBlockchainWordAsUint64(words[3]);
  return claim;
}

bool ChunkClaimsContract::CanEdit(int32_t x, int32_t y, const std::string& account) const
{
  std::string resultHex = {};
  if (!Call(
        BuildBlockchainCallData(
          SELECTOR_CAN_EDIT,
          {
            EncodeBlockchainInt32Argument(x),
            EncodeBlockchainInt32Argument(y),
            EncodeBlockchainAddressArgument(account)
          }
        ),
        &resultHex
      ))
  {
    return false;
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  return !words.empty() && DecodeBlockchainWordAsBool(words[0]);
}

ChunkEditState ChunkClaimsContract::CanEditWithRuntimeSigner(int32_t x, int32_t y, const std::string& actor) const
{
  ChunkEditState editState = {};

  std::string resultHex = {};
  if (!Call(
        BuildBlockchainCallData(
          SELECTOR_CAN_EDIT_WITH_RUNTIME_SIGNER,
          {
            EncodeBlockchainInt32Argument(x),
            EncodeBlockchainInt32Argument(y),
            EncodeBlockchainAddressArgument(actor)
          }
        ),
        &resultHex
      ))
  {
    return editState;
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  if (words.size() < 3) return editState;

  editState.available = true;
  editState.allowed = DecodeBlockchainWordAsBool(words[0]);
  editState.resolvedActor = DecodeBlockchainWordAsAddress(words[1]);
  editState.actorUsesRuntimeSession = DecodeBlockchainWordAsBool(words[2]);
  return editState;
}

uint64_t ChunkClaimsContract::EditorEpochOfChunk(int32_t x, int32_t y) const
{
  std::string resultHex = {};
  if (!Call(
        BuildBlockchainCallData(
          SELECTOR_EDITOR_EPOCH_OF_CHUNK,
          {EncodeBlockchainInt32Argument(x), EncodeBlockchainInt32Argument(y)}
        ),
        &resultHex
      ))
  {
    return 0;
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  return words.empty() ? 0 : DecodeBlockchainWordAsUint64(words[0]);
}

ChunkRuntimeState ChunkClaimsContract::GetChunkRuntimeState(int32_t x, int32_t y, const std::string& actor) const
{
  ChunkRuntimeState state = {};

  std::string resultHex = {};
  if (!Call(
        BuildBlockchainCallData(
          SELECTOR_GET_CHUNK_RUNTIME_STATE,
          {
            EncodeBlockchainInt32Argument(x),
            EncodeBlockchainInt32Argument(y),
            EncodeBlockchainAddressArgument(actor)
          }
        ),
        &resultHex
      ))
  {
    return state;
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  if (words.size() < 12) return state;

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
  state.actorUsesRuntimeSession = DecodeBlockchainWordAsBool(words[11]);
  return state;
}

bool ChunkClaimsContract::IsRegisteredPlayer(const std::string& account) const
{
  std::string resultHex = {};
  if (!Call(BuildBlockchainCallData(SELECTOR_IS_REGISTERED_PLAYER, {EncodeBlockchainAddressArgument(account)}), &resultHex))
  {
    return false;
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  return !words.empty() && DecodeBlockchainWordAsBool(words[0]);
}
