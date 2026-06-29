#pragma once

#include "SmartContract.h"

#include <cstdint>
#include <string>

struct ResolvedRuntimeAccount
{
  bool available = false;
  std::string account = {};
  bool isDirect = false;
  bool isRuntimeSession = false;
};

struct RuntimeSessionState
{
  bool available = false;
  std::string account = {};
  uint64_t expiresAt = 0;
  bool active = false;
};

class PlayerRegistryContract : public SmartContract
{
 public:
  using SmartContract::SmartContract;

  bool IsRegistered(const std::string& account) const;
  std::string PlayerIdOf(const std::string& account) const;
  ResolvedRuntimeAccount ResolveRuntimeAccount(const std::string& actor) const;
  RuntimeSessionState GetRuntimeSession(const std::string& session) const;

 protected:
  const char* GetContractName() const override;
};
