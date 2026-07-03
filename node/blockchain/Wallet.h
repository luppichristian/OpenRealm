#pragma once

#include <string>

class Wallet
{
 public:
  Wallet() = default;
  explicit Wallet(std::string accountAddress, std::string runtimeSignerAddress = {});

  void Connect(std::string accountAddress, std::string runtimeSignerAddress = {});
  void Disconnect();

  bool IsConnected() const;
  bool CanTransact() const;
  const std::string& GetAccountAddress() const;
  const std::string& GetRuntimeSignerAddress() const;
  std::string GetActiveSignerAddress() const;
  std::string GetTransactionSenderAddress() const;
  std::string DescribeWallet() const;

 private:
  bool connected = false;
  std::string accountAddress = {};
  std::string runtimeSignerAddress = {};
};
