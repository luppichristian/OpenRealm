#pragma once

#include "BlockchainConfig.h"
#include "Wallet.h"

#include <string>
#include <vector>

struct BlockchainTransactionLog
{
  std::string address = {};
  std::vector<std::string> topics = {};
  std::string data = {};
};

struct BlockchainTransactionReceipt
{
  bool available = false;
  bool success = false;
  std::string transactionHash = {};
  std::string blockNumber = {};
  std::string gasUsed = {};
  std::string status = {};
  std::string contractAddress = {};
  std::vector<BlockchainTransactionLog> logs = {};
};

class BlockchainRpcClient
{
 public:
  explicit BlockchainRpcClient(BlockchainConfig config);

  const BlockchainConfig& GetConfig() const;
  const std::string& GetRpcUrl() const;
  std::string DescribeStack() const;
  std::string EthChainId() const;
  bool EthCall(
      const std::string& contractAddress,
      const std::string& callData,
      std::string* resultHex,
      const std::string& fromAddress = {},
      const std::string& valueHex = {}) const;
  bool EthSendTransaction(
      const std::string& fromAddress,
      const std::string& contractAddress,
      const std::string& callData,
      std::string* transactionHash,
      const std::string& valueHex = {}) const;
  bool EthSendTransaction(
      const Wallet& wallet,
      const std::string& contractAddress,
      const std::string& callData,
      std::string* transactionHash,
      const std::string& valueHex = {}) const;
  bool EthGetTransactionReceipt(const std::string& transactionHash, BlockchainTransactionReceipt* receipt) const;
  bool WaitForTransactionReceipt(
      const std::string& transactionHash,
      BlockchainTransactionReceipt* receipt,
      int timeoutMilliseconds = 15000,
      int pollMilliseconds = 200) const;

 private:
  BlockchainConfig config = {};
};
