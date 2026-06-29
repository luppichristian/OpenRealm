#pragma once

#include <string>

struct WalletState
{
  bool connected = false;
  std::string accountAddress = {};
  std::string runtimeSignerAddress = {};
};

class Wallet
{
 public:
  Wallet() = default;
  explicit Wallet(WalletState walletState);

  const WalletState& GetState() const;
  bool IsConnected() const;
  const std::string& GetAccountAddress() const;
  const std::string& GetRuntimeSignerAddress() const;
  std::string GetActiveSignerAddress() const;
  std::string DescribeWallet() const;

 private:
  WalletState state = {};
};
