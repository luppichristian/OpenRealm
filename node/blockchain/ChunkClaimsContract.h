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

struct ChunkEditState
{
  bool available = false;
  bool allowed = false;
  std::string resolvedActor = {};
  bool actorUsesRuntimeSession = false;
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

class ChunkClaimsContract : public SmartContract
{
 public:
  using SmartContract::SmartContract;

  std::string OwnerOf(int32_t x, int32_t y) const;
  std::string TokenIdOfChunk(int32_t x, int32_t y) const;
  ChunkClaimState GetClaim(int32_t x, int32_t y) const;
  ChunkClaimState GetClaimByTokenId(uint64_t tokenId) const;
  bool CanEdit(int32_t x, int32_t y, const std::string& account) const;
  ChunkEditState CanEditWithRuntimeSigner(int32_t x, int32_t y, const std::string& actor) const;
  uint64_t EditorEpochOfChunk(int32_t x, int32_t y) const;
  ChunkRuntimeState GetChunkRuntimeState(int32_t x, int32_t y, const std::string& actor) const;
  bool IsRegisteredPlayer(const std::string& account) const;

 protected:
  const char* GetContractName() const override;
};
