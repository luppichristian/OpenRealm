#pragma once

#include "SmartContract.h"

#include <cstdint>
#include <string>

struct PlayerProfileState
{
  bool available = false;
  std::string playerId = {};
  std::string handle = {};
  std::string metadataUri = {};
  bool active = false;
};

struct ResolvedRuntimeAccount
{
  bool available = false;
  std::string account = {};
  bool isDirect = false;
  bool isRuntimeSession = false;
};

struct RuntimeSessionState
{
  bool available = false;
  std::string account = {};
  uint64_t expiresAt = 0;
  bool active = false;
};

class PlayerRegistryContract : public SmartContract
{
 public:
  using SmartContract::SmartContract;

  std::string GetOwner() const;
  BlockchainTransactionReceipt TransferOwnership(const std::string& newOwner) const;
  uint64_t GetNextPlayerId() const;
  BlockchainTransactionReceipt Register(const std::string& handle, const std::string& metadataUri) const;
  BlockchainTransactionReceipt Unregister() const;
  BlockchainTransactionReceipt UpdateMetadataURI(const std::string& metadataUri) const;
  BlockchainTransactionReceipt UpdateHandle(const std::string& newHandle) const;
  BlockchainTransactionReceipt AuthorizeRuntimeSession(const std::string& session, uint64_t expiresAt) const;
  BlockchainTransactionReceipt RevokeRuntimeSession(const std::string& session) const;
  PlayerProfileState GetProfile(const std::string& account) const;
  RuntimeSessionState GetRuntimeSession(const std::string& session) const;
  std::string PlayerIdOf(const std::string& account) const;
  bool IsRegistered(const std::string& account) const;
  std::string AccountForHandle(const std::string& handle) const;
  bool HandleHashAvailable(const std::string& handleHash) const;
  ResolvedRuntimeAccount ResolveRuntimeAccount(const std::string& actor) const;

 protected:
  const char* GetContractName() const override;
};
