#include "PlayerRegistryContract.h"

#include <vector>

static constexpr const char* SELECTOR_IS_REGISTERED = "0xc224122d";
static constexpr const char* SELECTOR_PLAYER_ID_OF = "0xc59c689c";
static constexpr const char* SELECTOR_RESOLVE_RUNTIME_ACCOUNT = "0x935fe2d1";
static constexpr const char* SELECTOR_GET_RUNTIME_SESSION = "0x1c2cf579";

const char* PlayerRegistryContract::GetContractName() const
{
  return "PlayerRegistry";
}

bool PlayerRegistryContract::IsRegistered(const std::string& account) const
{
  std::string resultHex = {};
  if (!Call(BuildBlockchainCallData(SELECTOR_IS_REGISTERED, {EncodeBlockchainAddressArgument(account)}), &resultHex))
  {
    return false;
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  return !words.empty() && DecodeBlockchainWordAsBool(words[0]);
}

std::string PlayerRegistryContract::PlayerIdOf(const std::string& account) const
{
  std::string resultHex = {};
  if (!Call(BuildBlockchainCallData(SELECTOR_PLAYER_ID_OF, {EncodeBlockchainAddressArgument(account)}), &resultHex))
  {
    return {};
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  return words.empty() ? std::string{} : DecodeBlockchainWordAsUintHex(words[0]);
}

ResolvedRuntimeAccount PlayerRegistryContract::ResolveRuntimeAccount(const std::string& actor) const
{
  ResolvedRuntimeAccount resolved = {};

  std::string resultHex = {};
  if (!Call(BuildBlockchainCallData(SELECTOR_RESOLVE_RUNTIME_ACCOUNT, {EncodeBlockchainAddressArgument(actor)}), &resultHex))
  {
    return resolved;
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  if (words.size() < 3) return resolved;

  resolved.available = true;
  resolved.account = DecodeBlockchainWordAsAddress(words[0]);
  resolved.isDirect = DecodeBlockchainWordAsBool(words[1]);
  resolved.isRuntimeSession = DecodeBlockchainWordAsBool(words[2]);
  return resolved;
}

RuntimeSessionState PlayerRegistryContract::GetRuntimeSession(const std::string& session) const
{
  RuntimeSessionState runtimeSession = {};

  std::string resultHex = {};
  if (!Call(BuildBlockchainCallData(SELECTOR_GET_RUNTIME_SESSION, {EncodeBlockchainAddressArgument(session)}), &resultHex))
  {
    return runtimeSession;
  }

  std::vector<std::string> words = SplitBlockchainWords(resultHex);
  if (words.size() < 3) return runtimeSession;

  runtimeSession.available = true;
  runtimeSession.account = DecodeBlockchainWordAsAddress(words[0]);
  runtimeSession.expiresAt = DecodeBlockchainWordAsUint64(words[1]);
  runtimeSession.active = DecodeBlockchainWordAsBool(words[2]);
  return runtimeSession;
}
