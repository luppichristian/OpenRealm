#include "Blockchain.h"

#include <utility>

Blockchain::Blockchain(BlockchainConfig blockchainConfig, Wallet connectedWallet)
    : config(std::move(blockchainConfig)), wallet(std::move(connectedWallet)), rpcClient(config), globalParams(&rpcClient, &wallet, config.globalParamsAddress), playerRegistry(&rpcClient, &wallet, config.playerRegistryAddress), chunkClaims(&rpcClient, &wallet, config.chunkClaimsAddress), marketplace(&rpcClient, &wallet, config.marketplaceAddress)
{
}

const BlockchainConfig& Blockchain::GetConfig() const
{
  return config;
}

const Wallet& Blockchain::GetWallet() const
{
  return wallet;
}

Wallet& Blockchain::GetWallet()
{
  return wallet;
}

const BlockchainRpcClient& Blockchain::GetRpcClient() const
{
  return rpcClient;
}

BlockchainRpcClient& Blockchain::GetRpcClient()
{
  return rpcClient;
}

GlobalParamsContract& Blockchain::GetGlobalParams()
{
  return globalParams;
}

const GlobalParamsContract& Blockchain::GetGlobalParams() const
{
  return globalParams;
}

PlayerRegistryContract& Blockchain::GetPlayerRegistry()
{
  return playerRegistry;
}

const PlayerRegistryContract& Blockchain::GetPlayerRegistry() const
{
  return playerRegistry;
}

ChunkClaimsContract& Blockchain::GetChunkClaims()
{
  return chunkClaims;
}

const ChunkClaimsContract& Blockchain::GetChunkClaims() const
{
  return chunkClaims;
}

MarketplaceContract& Blockchain::GetMarketplace()
{
  return marketplace;
}

const MarketplaceContract& Blockchain::GetMarketplace() const
{
  return marketplace;
}

std::string Blockchain::DescribeStack() const
{
  return "blockchain facade over " + rpcClient.DescribeStack();
}
