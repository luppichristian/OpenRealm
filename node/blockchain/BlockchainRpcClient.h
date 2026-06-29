#pragma once

#include <string>

class BlockchainRpcClient
{
 public:
  explicit BlockchainRpcClient(std::string rpcUrl);

  const std::string& GetRpcUrl() const;
  std::string DescribeStack() const;
  std::string EthChainId() const;

 private:
  std::string rpcUrl;
};
