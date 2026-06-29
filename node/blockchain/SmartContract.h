#pragma once

#include "BlockchainAbi.h"
#include "BlockchainRpcClient.h"

#include <string>
#include <utility>

class SmartContract
{
 public:
  SmartContract(const BlockchainRpcClient* rpcClient, std::string address)
      : rpc(rpcClient)
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

  bool HasContractAddress() const
  {
    return rpc != nullptr && !IsZeroBlockchainAddress(contractAddress);
  }

  bool Call(const std::string& callData, std::string* resultHex) const
  {
    return rpc != nullptr && rpc->EthCall(contractAddress, callData, resultHex);
  }

  virtual const char* GetContractName() const = 0;

 private:
  const BlockchainRpcClient* rpc = nullptr;
  std::string contractAddress = {};
};
