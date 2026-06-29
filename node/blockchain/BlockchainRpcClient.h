#pragma once

#include "BlockchainConfig.h"

#include <string>

class BlockchainRpcClient
{
 public:
  explicit BlockchainRpcClient(BlockchainConfig config);

  const BlockchainConfig& GetConfig() const;
  const std::string& GetRpcUrl() const;
  std::string DescribeStack() const;
  std::string EthChainId() const;
  bool EthCall(const std::string& contractAddress, const std::string& callData, std::string* resultHex) const;

 private:
  BlockchainConfig config = {};
};
