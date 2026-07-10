#include "Wallet.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace
{
std::string NormalizeWalletAddress(std::string value)
{
  if (value.empty())
  {
    return {};
  }

  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });

  if (value.rfind("0x", 0) != 0)
  {
    value = "0x" + value;
  }

  return value == "0x0000000000000000000000000000000000000000" ? std::string {} : value;
}
}

Wallet::Wallet(std::string accountAddress)
{
  Connect(std::move(accountAddress));
}

Wallet::Wallet(std::string accountAddress, std::string privateKey)
{
  Connect(std::move(accountAddress), std::move(privateKey));
}

void Wallet::Connect(std::string accountAddressValue, std::string privateKeyValue)
{
  accountAddress = NormalizeWalletAddress(std::move(accountAddressValue));
  privateKey = std::move(privateKeyValue);
  connected = !accountAddress.empty() || !privateKey.empty();
}

void Wallet::Disconnect()
{
  connected = false;
  accountAddress.clear();
  privateKey.clear();
}

bool Wallet::IsConnected() const
{
  return connected;
}

bool Wallet::HasAccountAddress() const
{
  return !accountAddress.empty();
}

bool Wallet::HasPrivateKey() const
{
  return !privateKey.empty();
}

bool Wallet::CanTransact() const
{
  return connected && (HasAccountAddress() || HasPrivateKey());
}

const std::string& Wallet::GetAccountAddress() const
{
  return accountAddress;
}

const std::string& Wallet::GetPrivateKey() const
{
  return privateKey;
}

std::string Wallet::GetTransactionSenderAddress() const
{
  if (!HasAccountAddress())
  {
    return {};
  }

  return accountAddress;
}

std::string Wallet::DescribeWallet() const
{
  if (!connected)
  {
    return "disconnected";
  }

  std::ostringstream stream;
  if (HasAccountAddress())
  {
    stream << accountAddress;
  }
  else
  {
    stream << "<address-unset>";
  }

  if (HasPrivateKey())
  {
    stream << " (signing enabled)";
  }
  else
  {
    stream << " (rpc-managed signing only)";
  }

  return stream.str();
}
