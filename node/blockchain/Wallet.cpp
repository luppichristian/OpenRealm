#include "Wallet.h"

#include <utility>

Wallet::Wallet(WalletState walletState)
    : state(std::move(walletState))
{
}

const WalletState& Wallet::GetState() const
{
  return state;
}

bool Wallet::IsConnected() const
{
  return state.connected && !state.accountAddress.empty();
}

const std::string& Wallet::GetAccountAddress() const
{
  return state.accountAddress;
}

const std::string& Wallet::GetRuntimeSignerAddress() const
{
  return state.runtimeSignerAddress;
}

std::string Wallet::GetActiveSignerAddress() const
{
  if (!state.runtimeSignerAddress.empty()) return state.runtimeSignerAddress;
  return state.accountAddress;
}

std::string Wallet::DescribeWallet() const
{
  if (!IsConnected()) return "wallet disconnected";
  if (state.runtimeSignerAddress.empty() || state.runtimeSignerAddress == state.accountAddress)
  {
    return "wallet connected";
  }
  return "wallet connected with runtime signer";
}
