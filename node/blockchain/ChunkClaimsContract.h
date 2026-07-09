#pragma once

#include "SmartContract.h"

#include <cstdint>
#include <string>

struct ChunkClaimState
{
  bool available = false;
  std::string tokenId = {};
  int32_t x = 0;
  int32_t y = 0;
  uint64_t claimedAt = 0;
};

struct ChunkRuntimeState
{
  bool available = false;
  bool claimed = false;
  std::string tokenId = {};
  int32_t x = 0;
  int32_t y = 0;
  std::string owner = {};
  std::string ownerPlayerId = {};
  uint64_t claimedAt = 0;
  uint64_t editorEpoch = 0;
  std::string resolvedActor = {};
  bool actorRegistered = false;
  bool actorCanEdit = false;
  bool actorUsesRuntimeSession = false;
};

struct ChunkClaimTransaction
{
  std::string tokenId = {};
  BlockchainTransactionReceipt receipt = {};
};

class ChunkClaimsContract : public SmartContract
{
 public:
  using SmartContract::SmartContract;

  std::string GetOwner() const;
  BlockchainTransactionReceipt TransferOwnership(const std::string& newOwner) const;
  std::string GetRegistryAddress() const;
  std::string GetGlobalParamsAddress() const;
  std::string GetMarketplaceAddress() const;
  uint64_t GetNextTokenId() const;
  BlockchainTransactionReceipt SetMarketplace(const std::string& marketplaceAddress) const;
  ChunkClaimTransaction ClaimChunk(int32_t x, int32_t y, const std::string& paymentValue) const;
  BlockchainTransactionReceipt AbandonChunk(int32_t x, int32_t y) const;
  BlockchainTransactionReceipt TransferChunk(int32_t x, int32_t y, const std::string& newOwner) const;
  BlockchainTransactionReceipt SetChunkEditor(int32_t x, int32_t y, const std::string& editor, bool allowed) const;
  BlockchainTransactionReceipt MarketplaceTransferChunk(
      int32_t x,
      int32_t y,
      const std::string& from,
      const std::string& to) const;
  std::string OwnerOf(int32_t x, int32_t y) const;
  std::string TokenIdOfChunk(int32_t x, int32_t y) const;
  ChunkClaimState GetClaim(int32_t x, int32_t y) const;
  ChunkClaimState GetClaimByTokenId(const std::string& tokenId) const;
  bool CanEdit(int32_t x, int32_t y, const std::string& account) const;
  uint64_t EditorEpochOfChunk(int32_t x, int32_t y) const;
  ChunkRuntimeState GetChunkRuntimeState(int32_t x, int32_t y, const std::string& actor) const;
  bool IsRegisteredPlayer(const std::string& account) const;
  std::string EncodeChunkKey(int32_t x, int32_t y) const;
  std::string GetName() const;
  std::string GetSymbol() const;
  std::string BalanceOf(const std::string& owner) const;
  std::string OwnerOfToken(const std::string& tokenId) const;
  std::string GetApproved(const std::string& tokenId) const;
  bool IsApprovedForAll(const std::string& owner, const std::string& operatorAddress) const;
  BlockchainTransactionReceipt Approve(const std::string& to, const std::string& tokenId) const;
  BlockchainTransactionReceipt SetApprovalForAll(const std::string& operatorAddress, bool approved) const;
  BlockchainTransactionReceipt TransferFrom(
      const std::string& from,
      const std::string& to,
      const std::string& tokenId) const;
  std::string TokenURI(const std::string& tokenId) const;

 protected:
  const char* GetContractName() const override;
};
