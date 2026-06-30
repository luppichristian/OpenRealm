#include "PlayerRegistryContract.h"

#include <vector>

static constexpr const char* SELECTOR_OWNER = "0x8da5cb5b";
static constexpr const char* SELECTOR_TRANSFER_OWNERSHIP = "0xf2fde38b";
static constexpr const char* SELECTOR_NEXT_PLAYER_ID = "0x19863f35";
static constexpr const char* SELECTOR_REGISTER = "0xd6192e29";
static constexpr const char* SELECTOR_UNREGISTER = "0xe92553d2";
static constexpr const char* SELECTOR_UPDATE_METADATA_URI = "0x079bef11";
static constexpr const char* SELECTOR_UPDATE_HANDLE = "0x65380f43";
static constexpr const char* SELECTOR_AUTHORIZE_RUNTIME_SESSION = "0x54294bde";
static constexpr const char* SELECTOR_REVOKE_RUNTIME_SESSION = "0x4ef5b029";
static constexpr const char* SELECTOR_GET_PROFILE = "0xaae68574";
static constexpr const char* SELECTOR_GET_RUNTIME_SESSION = "0x1c2cf579";
static constexpr const char* SELECTOR_PLAYER_ID_OF = "0xc59c689c";
static constexpr const char* SELECTOR_IS_REGISTERED = "0xc224122d";
static constexpr const char* SELECTOR_ACCOUNT_FOR_HANDLE = "0x28275aef";
static constexpr const char* SELECTOR_HANDLE_HASH_AVAILABLE = "0xd47f893e";
static constexpr const char* SELECTOR_RESOLVE_RUNTIME_ACCOUNT = "0x935fe2d1";

const char* PlayerRegistryContract::GetContractName() const
{
  return "PlayerRegistry";
}

std::string PlayerRegistryContract::GetOwner() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_OWNER, {}), &words) || words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsAddress(words[0]);
}

BlockchainTransactionReceipt PlayerRegistryContract::TransferOwnership(const std::string& newOwner) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(SELECTOR_TRANSFER_OWNERSHIP, {EncodeBlockchainAddressArgument(newOwner)}),
      &receipt);
  return receipt;
}

uint64_t PlayerRegistryContract::GetNextPlayerId() const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_NEXT_PLAYER_ID, {}), &words) || words.empty())
  {
    return 0;
  }
  return DecodeBlockchainWordAsUint64(words[0]);
}

BlockchainTransactionReceipt PlayerRegistryContract::Register(const std::string& handle, const std::string& metadataUri) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(
          SELECTOR_REGISTER,
          {EncodeBlockchainStringArgument(handle), EncodeBlockchainStringArgument(metadataUri)}),
      &receipt);
  return receipt;
}

BlockchainTransactionReceipt PlayerRegistryContract::Unregister() const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(BuildBlockchainCallData(SELECTOR_UNREGISTER, {}), &receipt);
  return receipt;
}

BlockchainTransactionReceipt PlayerRegistryContract::UpdateMetadataURI(const std::string& metadataUri) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(SELECTOR_UPDATE_METADATA_URI, {EncodeBlockchainStringArgument(metadataUri)}),
      &receipt);
  return receipt;
}

BlockchainTransactionReceipt PlayerRegistryContract::UpdateHandle(const std::string& newHandle) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(SELECTOR_UPDATE_HANDLE, {EncodeBlockchainStringArgument(newHandle)}),
      &receipt);
  return receipt;
}

BlockchainTransactionReceipt PlayerRegistryContract::AuthorizeRuntimeSession(const std::string& session, uint64_t expiresAt) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(
          SELECTOR_AUTHORIZE_RUNTIME_SESSION,
          {EncodeBlockchainAddressArgument(session), EncodeBlockchainUint64Argument(expiresAt)}),
      &receipt);
  return receipt;
}

BlockchainTransactionReceipt PlayerRegistryContract::RevokeRuntimeSession(const std::string& session) const
{
  BlockchainTransactionReceipt receipt = {};
  SendTransaction(
      BuildBlockchainCallData(SELECTOR_REVOKE_RUNTIME_SESSION, {EncodeBlockchainAddressArgument(session)}),
      &receipt);
  return receipt;
}

PlayerProfileState PlayerRegistryContract::GetProfile(const std::string& account) const
{
  PlayerProfileState profile = {};

  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(SELECTOR_GET_PROFILE, {EncodeBlockchainAddressArgument(account)}),
          &words) ||
      words.size() < 4)
  {
    return profile;
  }

  profile.available = true;
  profile.playerId = DecodeBlockchainWordAsUintHex(words[0]);
  profile.handle = DecodeBlockchainStringAt(words, 1);
  profile.metadataUri = DecodeBlockchainStringAt(words, 2);
  profile.active = DecodeBlockchainWordAsBool(words[3]);
  return profile;
}

RuntimeSessionState PlayerRegistryContract::GetRuntimeSession(const std::string& session) const
{
  RuntimeSessionState runtimeSession = {};

  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(SELECTOR_GET_RUNTIME_SESSION, {EncodeBlockchainAddressArgument(session)}),
          &words) ||
      words.size() < 3)
  {
    return runtimeSession;
  }

  runtimeSession.available = true;
  runtimeSession.account = DecodeBlockchainWordAsAddress(words[0]);
  runtimeSession.expiresAt = DecodeBlockchainWordAsUint64(words[1]);
  runtimeSession.active = DecodeBlockchainWordAsBool(words[2]);
  return runtimeSession;
}

std::string PlayerRegistryContract::PlayerIdOf(const std::string& account) const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_PLAYER_ID_OF, {EncodeBlockchainAddressArgument(account)}), &words) ||
      words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsUintHex(words[0]);
}

bool PlayerRegistryContract::IsRegistered(const std::string& account) const
{
  std::vector<std::string> words = {};
  if (!CallWords(BuildBlockchainCallData(SELECTOR_IS_REGISTERED, {EncodeBlockchainAddressArgument(account)}), &words) ||
      words.empty())
  {
    return false;
  }
  return DecodeBlockchainWordAsBool(words[0]);
}

std::string PlayerRegistryContract::AccountForHandle(const std::string& handle) const
{
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(SELECTOR_ACCOUNT_FOR_HANDLE, {EncodeBlockchainStringArgument(handle)}),
          &words) ||
      words.empty())
  {
    return {};
  }
  return DecodeBlockchainWordAsAddress(words[0]);
}

bool PlayerRegistryContract::HandleHashAvailable(const std::string& handleHash) const
{
  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(SELECTOR_HANDLE_HASH_AVAILABLE, {EncodeBlockchainBytes32Argument(handleHash)}),
          &words) ||
      words.empty())
  {
    return false;
  }
  return DecodeBlockchainWordAsBool(words[0]);
}

ResolvedRuntimeAccount PlayerRegistryContract::ResolveRuntimeAccount(const std::string& actor) const
{
  ResolvedRuntimeAccount resolved = {};

  std::vector<std::string> words = {};
  if (!CallWords(
          BuildBlockchainCallData(SELECTOR_RESOLVE_RUNTIME_ACCOUNT, {EncodeBlockchainAddressArgument(actor)}),
          &words) ||
      words.size() < 3)
  {
    return resolved;
  }

  resolved.available = true;
  resolved.account = DecodeBlockchainWordAsAddress(words[0]);
  resolved.isDirect = DecodeBlockchainWordAsBool(words[1]);
  resolved.isRuntimeSession = DecodeBlockchainWordAsBool(words[2]);
  return resolved;
}
