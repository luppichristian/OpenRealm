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
  SmartContract(const BlockchainRpcClient* rpcClient, const Wallet* walletState, std::string address)
      : rpc(rpcClient)
      , wallet(walletState)
      , contractAddress(std::move(address))
  {
  }

  virtual ~SmartContract() = default;

  const std::string& GetContractAddress() const
  {
    return contractAddress;
  }

  std::string DescribeContract() const
  {
    return std::string(GetContractName()) + " contract wrapper";
  }

 protected:
  const BlockchainRpcClient* GetRpcClient() const
  {
    return rpc;
  }

  const Wallet* GetWallet() const
  {
    return wallet;
  }

  bool HasContractAddress() const
  {
    return rpc != nullptr && !IsZeroBlockchainAddress(contractAddress);
  }

  std::string GetWalletTransactionSender() const
  {
    return wallet != nullptr ? wallet->GetTransactionSenderAddress() : std::string{};
  }

  std::string GetWalletActiveSigner() const
  {
    return wallet != nullptr ? wallet->GetActiveSignerAddress() : std::string{};
  }

  bool Call(
      const std::string& callData,
      std::string* resultHex,
      const std::string& fromAddress = {},
      const std::string& valueHex = {}
  ) const
  {
    return rpc != nullptr && rpc->EthCall(contractAddress, callData, resultHex, fromAddress, valueHex);
  }

  bool CallWords(
      const std::string& callData,
      std::vector<std::string>* words,
      const std::string& fromAddress = {},
      const std::string& valueHex = {}
  ) const
  {
    if (words == nullptr)
    {
      return false;
    }

    std::string resultHex = {};
    if (!Call(callData, &resultHex, fromAddress, valueHex))
    {
      return false;
    }

    *words = SplitBlockchainWords(resultHex);
    return !words->empty() || resultHex == "0x";
  }

  bool SendTransaction(
      const std::string& callData,
      BlockchainTransactionReceipt* receipt,
      const std::string& valueHex = {},
      const std::string& fromAddress = {}
  ) const
  {
    if (rpc == nullptr || receipt == nullptr)
    {
      return false;
    }

    std::string sender = fromAddress.empty() ? GetWalletTransactionSender() : fromAddress;
    if (IsZeroBlockchainAddress(sender))
    {
      return false;
    }

    std::string transactionHash = {};
    if (!rpc->EthSendTransaction(sender, contractAddress, callData, &transactionHash, valueHex))
    {
      return false;
    }

    receipt->transactionHash = transactionHash;
    return rpc->WaitForTransactionReceipt(transactionHash, receipt);
  }

  virtual const char* GetContractName() const = 0;

 private:
  const BlockchainRpcClient* rpc = nullptr;
  const Wallet* wallet = nullptr;
  std::string contractAddress = {};
};
