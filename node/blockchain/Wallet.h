#pragma once

#include <string>

class Wallet
{
 public:
  Wallet() = default;
  explicit Wallet(std::string accountAddress);

  void Connect(std::string accountAddress);
  void Disconnect();

  bool IsConnected() const;
  bool CanTransact() const;
  const std::string& GetAccountAddress() const;
  std::string GetTransactionSenderAddress() const;
  std::string DescribeWallet() const;

 private:
  bool connected = false;
  std::string accountAddress = {};
};
