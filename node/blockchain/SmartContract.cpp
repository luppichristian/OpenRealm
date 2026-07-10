#include "SmartContract.h"

SmartContract::SmartContract(const BlockchainRpcClient* rpcClient, const Wallet* walletState, std::string address)
    : rpc(rpcClient), wallet(walletState), contractAddress(std::move(address))
{
}

const std::string& SmartContract::GetContractAddress() const
{
  return contractAddress;
}

std::string SmartContract::DescribeContract() const
{
  return std::string(GetContractName()) + " contract wrapper";
}

const BlockchainRpcClient* SmartContract::GetRpcClient() const
{
  return rpc;
}

const Wallet* SmartContract::GetWallet() const
{
  return wallet;
}

bool SmartContract::HasContractAddress() const
{
  return rpc != nullptr && !IsZeroBlockchainAddress(contractAddress);
}

std::string SmartContract::GetWalletTransactionSender() const
{
  return wallet != nullptr ? wallet->GetTransactionSenderAddress() : std::string {};
}

bool SmartContract::Call(
    const std::string& callData,
    std::string* resultHex,
    const std::string& fromAddress,
    const std::string& valueHex) const
{
  return rpc != nullptr && rpc->EthCall(contractAddress, callData, resultHex, fromAddress, valueHex);
}

bool SmartContract::CallWords(
    const std::string& callData,
    std::vector<std::string>* words,
    const std::string& fromAddress,
    const std::string& valueHex) const
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

bool SmartContract::SendTransaction(
    const std::string& callData,
    BlockchainTransactionReceipt* receipt,
    const std::string& valueHex,
    const std::string& fromAddress) const
{
  if (rpc == nullptr || receipt == nullptr)
  {
    return false;
  }

  std::string transactionHash = {};
  if (!fromAddress.empty())
  {
    if (IsZeroBlockchainAddress(fromAddress) ||
        !rpc->EthSendTransaction(fromAddress, contractAddress, callData, &transactionHash, valueHex))
    {
      return false;
    }
  }
  else if (wallet != nullptr)
  {
    if (!rpc->EthSendTransaction(*wallet, contractAddress, callData, &transactionHash, valueHex))
    {
      return false;
    }
  }
  else
  {
    return false;
  }

  receipt->transactionHash = transactionHash;
  return rpc->WaitForTransactionReceipt(transactionHash, receipt);
}
