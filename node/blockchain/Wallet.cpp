#include "Wallet.h"

#include "BlockchainAbi.h"

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
  return state.connected;
}

bool Wallet::CanTransact() const
{
  return state.connected && !IsZeroBlockchainAddress(state.accountAddress);
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
  if (!IsZeroBlockchainAddress(state.runtimeSignerAddress))
  {
    return NormalizeBlockchainAddress(state.runtimeSignerAddress);
  }

  return NormalizeBlockchainAddress(state.accountAddress);
}

std::string Wallet::GetTransactionSenderAddress() const
{
  return NormalizeBlockchainAddress(state.accountAddress);
}

std::string Wallet::DescribeWallet() const
{
  if (!state.connected)
  {
    return "wallet disconnected";
  }

  if (IsZeroBlockchainAddress(state.accountAddress))
  {
    return "wallet connected without account";
  }

  if (IsZeroBlockchainAddress(state.runtimeSignerAddress) ||
      NormalizeBlockchainAddress(state.runtimeSignerAddress) == NormalizeBlockchainAddress(state.accountAddress))
  {
    return "wallet connected";
  }

  return "wallet connected with delegated runtime signer";
}
