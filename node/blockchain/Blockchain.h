#pragma once

#include "BlockchainConfig.h"
#include "BlockchainRpcClient.h"
#include "ChunkClaimsContract.h"
#include "GlobalParamsContract.h"
#include "MarketplaceContract.h"
#include "PlayerRegistryContract.h"
#include "Wallet.h"

class Blockchain
{
 public:
  Blockchain(BlockchainConfig blockchainConfig, Wallet wallet);

  const BlockchainConfig& GetConfig() const;
  const Wallet& GetWallet() const;
  const BlockchainRpcClient& GetRpcClient() const;

  GlobalParamsContract& GetGlobalParams();
  const GlobalParamsContract& GetGlobalParams() const;
  PlayerRegistryContract& GetPlayerRegistry();
  const PlayerRegistryContract& GetPlayerRegistry() const;
  ChunkClaimsContract& GetChunkClaims();
  const ChunkClaimsContract& GetChunkClaims() const;
  MarketplaceContract& GetMarketplace();
  const MarketplaceContract& GetMarketplace() const;
  std::string DescribeStack() const;

 private:
  BlockchainConfig config = {};
  Wallet wallet = {};
  BlockchainRpcClient rpcClient;
  GlobalParamsContract globalParams;
  PlayerRegistryContract playerRegistry;
  ChunkClaimsContract chunkClaims;
  MarketplaceContract marketplace;
};
