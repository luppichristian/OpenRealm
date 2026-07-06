#include "Wallet.h"

#include "BlockchainAbi.h"

#include <utility>

Wallet::Wallet(std::string accountAddress)
{
  Connect(std::move(accountAddress));
}

void Wallet::Connect(std::string accountAddressValue)
{
  accountAddress = std::move(accountAddressValue);
  connected = !IsZeroBlockchainAddress(accountAddress);
}

void Wallet::Disconnect()
{
  connected = false;
  accountAddress.clear();
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

  return "wallet connected";
}
