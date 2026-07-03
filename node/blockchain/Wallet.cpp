#include "Wallet.h"

#include "BlockchainAbi.h"

#include <utility>

Wallet::Wallet(std::string accountAddress, std::string runtimeSignerAddress)
{
  Connect(std::move(accountAddress), std::move(runtimeSignerAddress));
}

void Wallet::Connect(std::string accountAddressValue, std::string runtimeSignerAddressValue)
{
  accountAddress = std::move(accountAddressValue);
  runtimeSignerAddress = std::move(runtimeSignerAddressValue);
  connected = !IsZeroBlockchainAddress(accountAddress);
}

void Wallet::Disconnect()
{
  connected = false;
  accountAddress.clear();
  runtimeSignerAddress.clear();
}

bool Wallet::IsConnected() const
{
  return connected;
}

bool Wallet::CanTransact() const
{
  return connected && !IsZeroBlockchainAddress(accountAddress);
}

const std::string& Wallet::GetAccountAddress() const
{
  return accountAddress;
}

const std::string& Wallet::GetRuntimeSignerAddress() const
{
  return runtimeSignerAddress;
}

std::string Wallet::GetActiveSignerAddress() const
{
  if (!IsZeroBlockchainAddress(runtimeSignerAddress))
  {
    return NormalizeBlockchainAddress(runtimeSignerAddress);
  }

  return NormalizeBlockchainAddress(accountAddress);
}

std::string Wallet::GetTransactionSenderAddress() const
{
  return NormalizeBlockchainAddress(accountAddress);
}

std::string Wallet::DescribeWallet() const
{
  if (!connected || IsZeroBlockchainAddress(accountAddress))
  {
    return "wallet disconnected";
  }

  if (IsZeroBlockchainAddress(runtimeSignerAddress) ||
      NormalizeBlockchainAddress(runtimeSignerAddress) == NormalizeBlockchainAddress(accountAddress))
  {
    return "wallet connected";
  }

  return "wallet connected with delegated runtime signer";
}
