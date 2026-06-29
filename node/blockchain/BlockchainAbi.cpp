#include "BlockchainAbi.h"

#include <algorithm>
#include <cstdio>

static std::string StripHexPrefix(const std::string& value)
{
  if (value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0)
  {
    return value.substr(2);
  }

  return value;
}

static std::string Lowercase(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
  {
    return (char)std::tolower(c);
  });
  return value;
}

static std::string LeftPad64(const std::string& value, char fill)
{
  if (value.size() >= 64)
  {
    return value.substr(value.size() - 64);
  }

  return std::string(64 - value.size(), fill) + value;
}

static std::string TrimHexWord(const std::string& value)
{
  size_t first = value.find_first_not_of('0');
  if (first == std::string::npos)
  {
    return "0";
  }

  return value.substr(first);
}

std::string NormalizeBlockchainAddress(const std::string& address)
{
  std::string normalized = Lowercase(StripHexPrefix(address));
  if (normalized.size() > 40)
  {
    normalized = normalized.substr(normalized.size() - 40);
  }

  if (normalized.empty())
  {
    normalized = std::string(40, '0');
  }
  else if (normalized.size() < 40)
  {
    normalized = std::string(40 - normalized.size(), '0') + normalized;
  }

  return "0x" + normalized;
}

bool IsZeroBlockchainAddress(const std::string& address)
{
  return NormalizeBlockchainAddress(address) == "0x0000000000000000000000000000000000000000";
}

std::string EncodeBlockchainAddressArgument(const std::string& address)
{
  return std::string(24, '0') + StripHexPrefix(NormalizeBlockchainAddress(address));
}

std::string EncodeBlockchainInt32Argument(int32_t value)
{
  const uint32_t bits = (uint32_t)value;
  char hex[9] = {};
  std::snprintf(hex, sizeof(hex), "%08x", bits);
  return LeftPad64(hex, value < 0 ? 'f' : '0');
}

std::string EncodeBlockchainUint64Argument(uint64_t value)
{
  char hex[17] = {};
  std::snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)value);
  return LeftPad64(hex, '0');
}

std::string BuildBlockchainCallData(const std::string& selector, const std::vector<std::string>& encodedArguments)
{
  std::string data = "0x" + StripHexPrefix(Lowercase(selector));
  for (const std::string& argument : encodedArguments)
  {
    data += Lowercase(argument);
  }
  return data;
}

std::vector<std::string> SplitBlockchainWords(const std::string& encodedData)
{
  std::vector<std::string> words = {};
  std::string data = Lowercase(StripHexPrefix(encodedData));
  if (data.empty() || (data.size() % 64) != 0)
  {
    return words;
  }

  for (size_t offset = 0; offset < data.size(); offset += 64)
  {
    words.push_back(data.substr(offset, 64));
  }

  return words;
}

std::string DecodeBlockchainWordAsAddress(const std::string& word)
{
  if (word.size() != 64)
  {
    return NormalizeBlockchainAddress({});
  }

  return NormalizeBlockchainAddress(word.substr(24));
}

bool DecodeBlockchainWordAsBool(const std::string& word)
{
  return TrimHexWord(word) != "0";
}

int32_t DecodeBlockchainWordAsInt32(const std::string& word)
{
  if (word.size() != 64)
  {
    return 0;
  }

  const uint32_t raw = (uint32_t)std::stoul(word.substr(56, 8), nullptr, 16);
  return (int32_t)raw;
}

uint64_t DecodeBlockchainWordAsUint64(const std::string& word)
{
  if (word.size() != 64)
  {
    return 0;
  }

  return (uint64_t)std::stoull(word.substr(48, 16), nullptr, 16);
}

std::string DecodeBlockchainWordAsUintHex(const std::string& word)
{
  if (word.size() != 64)
  {
    return "0x0";
  }

  return "0x" + TrimHexWord(word);
}
