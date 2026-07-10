#pragma once

#include <string>

class Wallet
{
 public:
  Wallet() = default;
  explicit Wallet(std::string accountAddress);
  Wallet(std::string accountAddress, std::string privateKey);

  void Connect(std::string accountAddress, std::string privateKey = {});
  void Disconnect();

  bool IsConnected() const;
  bool HasAccountAddress() const;
  bool HasPrivateKey() const;
  bool CanTransact() const;
  const std::string& GetAccountAddress() const;
  const std::string& GetPrivateKey() const;
  std::string GetTransactionSenderAddress() const;
  std::string DescribeWallet() const;

 private:
  bool connected = false;
  std::string accountAddress = {};
  std::string privateKey = {};
};
