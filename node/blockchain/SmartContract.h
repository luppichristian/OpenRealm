#pragma once

#include "BlockchainAbi.h"
#include "BlockchainRpcClient.h"
#include "Wallet.h"

#include <string>
#include <utility>
#include <vector>

class SmartContract
{
 public:
  SmartContract(const BlockchainRpcClient* rpcClient, const Wallet* walletState, std::string address);
  virtual ~SmartContract() = default;

  const std::string& GetContractAddress() const;
  std::string DescribeContract() const;

 protected:
  const BlockchainRpcClient* GetRpcClient() const;
  const Wallet* GetWallet() const;
  bool HasContractAddress() const;
  std::string GetWalletTransactionSender() const;
  bool Call(
      const std::string& callData,
      std::string* resultHex,
      const std::string& fromAddress = {},
      const std::string& valueHex = {}
  ) const;
  bool CallWords(
      const std::string& callData,
      std::vector<std::string>* words,
      const std::string& fromAddress = {},
      const std::string& valueHex = {}
  ) const;
  bool SendTransaction(
      const std::string& callData,
      BlockchainTransactionReceipt* receipt,
      const std::string& valueHex = {},
      const std::string& fromAddress = {}
  ) const;

  virtual const char* GetContractName() const = 0;

 private:
  const BlockchainRpcClient* rpc = nullptr;
  const Wallet* wallet = nullptr;
  std::string contractAddress = {};
};
