#include "BlockchainAbi.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <stdexcept>
#include <utility>

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

static std::string RightPadToWordBoundary(const std::string& value)
{
  const size_t remainder = value.size() % 64;
  if (remainder == 0)
  {
    return value;
  }

  return value + std::string(64 - remainder, '0');
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

static bool IsHexDigitString(const std::string& value)
{
  for (char c : value)
  {
    if (!std::isxdigit((unsigned char)c))
    {
      return false;
    }
  }

  return true;
}

static std::string DecimalToHex(const std::string& decimalValue)
{
  if (decimalValue.empty())
  {
    return "0";
  }

  std::string digits = decimalValue;
  for (char c : digits)
  {
    if (!std::isdigit((unsigned char)c))
    {
      return "0";
    }
  }

  size_t first = digits.find_first_not_of('0');
  if (first == std::string::npos)
  {
    return "0";
  }
  digits = digits.substr(first);

  std::string hex = {};
  while (!digits.empty())
  {
    std::string quotient = {};
    int remainder = 0;

    for (char digit : digits)
    {
      int accumulator = remainder * 10 + (digit - '0');
      int quotientDigit = accumulator / 16;
      remainder = accumulator % 16;
      if (!quotient.empty() || quotientDigit != 0)
      {
        quotient.push_back((char)('0' + quotientDigit));
      }
    }

    hex.push_back("0123456789abcdef"[remainder]);
    digits = quotient;
  }

  std::reverse(hex.begin(), hex.end());
  return hex.empty() ? std::string("0") : hex;
}

static std::string BytesToHex(const std::string& bytes)
{
  static constexpr char kHex[] = "0123456789abcdef";

  std::string hex = {};
  hex.reserve(bytes.size() * 2);
  for (unsigned char byte : bytes)
  {
    hex.push_back(kHex[(byte >> 4) & 0x0f]);
    hex.push_back(kHex[byte & 0x0f]);
  }
  return hex;
}

static std::string HexToBytes(const std::string& hex)
{
  std::string normalized = Lowercase(StripHexPrefix(hex));
  if ((normalized.size() % 2) != 0 || !IsHexDigitString(normalized))
  {
    return {};
  }

  std::string bytes = {};
  bytes.reserve(normalized.size() / 2);
  for (size_t i = 0; i < normalized.size(); i += 2)
  {
    bytes.push_back((char)std::stoi(normalized.substr(i, 2), nullptr, 16));
  }
  return bytes;
}

static BlockchainAbiArgument MakeStaticArgument(const std::string& word)
{
  BlockchainAbiArgument argument = {};
  argument.dynamic = false;
  argument.headWord = word;
  return argument;
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

std::string NormalizeBlockchainBytes32(const std::string& value)
{
  std::string normalized = Lowercase(StripHexPrefix(value));
  if (normalized.size() > 64)
  {
    normalized = normalized.substr(normalized.size() - 64);
  }
  if (!IsHexDigitString(normalized))
  {
    return "0x" + std::string(64, '0');
  }
  if (normalized.size() < 64)
  {
    normalized = std::string(64 - normalized.size(), '0') + normalized;
  }
  return "0x" + normalized;
}

std::string NormalizeBlockchainUintHex(const std::string& value)
{
  std::string normalized = {};
  if (value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0)
  {
    normalized = Lowercase(StripHexPrefix(value));
    if (!IsHexDigitString(normalized))
    {
      return "0x0";
    }
  }
  else
  {
    normalized = DecimalToHex(value);
  }

  normalized = TrimHexWord(normalized);
  return "0x" + normalized;
}

bool IsZeroBlockchainAddress(const std::string& address)
{
  return NormalizeBlockchainAddress(address) == "0x0000000000000000000000000000000000000000";
}

BlockchainAbiArgument EncodeBlockchainAddressArgument(const std::string& address)
{
  return MakeStaticArgument(std::string(24, '0') + StripHexPrefix(NormalizeBlockchainAddress(address)));
}

BlockchainAbiArgument EncodeBlockchainBoolArgument(bool value)
{
  return MakeStaticArgument(LeftPad64(value ? "1" : "0", '0'));
}

BlockchainAbiArgument EncodeBlockchainInt32Argument(int32_t value)
{
  const uint32_t bits = (uint32_t)value;
  char hex[9] = {};
  std::snprintf(hex, sizeof(hex), "%08x", bits);
  return MakeStaticArgument(LeftPad64(hex, value < 0 ? 'f' : '0'));
}

BlockchainAbiArgument EncodeBlockchainUint64Argument(uint64_t value)
{
  char hex[17] = {};
  std::snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)value);
  return MakeStaticArgument(LeftPad64(hex, '0'));
}

BlockchainAbiArgument EncodeBlockchainUint96Argument(uint64_t value)
{
  char hex[17] = {};
  std::snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)value);
  return MakeStaticArgument(LeftPad64(hex, '0'));
}

BlockchainAbiArgument EncodeBlockchainUint256Argument(const std::string& value)
{
  return MakeStaticArgument(LeftPad64(StripHexPrefix(NormalizeBlockchainUintHex(value)), '0'));
}

BlockchainAbiArgument EncodeBlockchainBytes32Argument(const std::string& value)
{
  return MakeStaticArgument(StripHexPrefix(NormalizeBlockchainBytes32(value)));
}

BlockchainAbiArgument EncodeBlockchainStringArgument(const std::string& value)
{
  BlockchainAbiArgument argument = {};
  argument.dynamic = true;

  std::string hexBytes = BytesToHex(value);
  const uint64_t length = (uint64_t)value.size();
  argument.tailWords.push_back(EncodeBlockchainUint64Argument(length).headWord);

  std::string padded = RightPadToWordBoundary(hexBytes);
  if (!padded.empty())
  {
    for (size_t offset = 0; offset < padded.size(); offset += 64)
    {
      argument.tailWords.push_back(padded.substr(offset, 64));
    }
  }

  if (argument.tailWords.empty())
  {
    argument.tailWords.push_back(std::string(64, '0'));
  }

  return argument;
}

std::string BuildBlockchainCallData(const std::string& selector, const std::vector<BlockchainAbiArgument>& encodedArguments)
{
  std::string data = "0x" + StripHexPrefix(Lowercase(selector));

  size_t staticWordCount = encodedArguments.size();
  size_t dynamicOffsetBytes = staticWordCount * 32;
  std::vector<std::string> dynamicWords = {};

  for (const BlockchainAbiArgument& argument : encodedArguments)
  {
    if (!argument.dynamic)
    {
      data += Lowercase(argument.headWord);
      continue;
    }

    data += EncodeBlockchainUint64Argument((uint64_t)dynamicOffsetBytes).headWord;
    dynamicOffsetBytes += argument.tailWords.size() * 32;
    dynamicWords.insert(dynamicWords.end(), argument.tailWords.begin(), argument.tailWords.end());
  }

  for (const std::string& word : dynamicWords)
  {
    data += Lowercase(word);
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

std::string DecodeBlockchainWordAsBytes32Hex(const std::string& word)
{
  if (word.size() != 64)
  {
    return "0x" + std::string(64, '0');
  }

  return "0x" + Lowercase(word);
}

std::string DecodeBlockchainStringAt(const std::vector<std::string>& words, size_t index)
{
  if (index >= words.size())
  {
    return {};
  }

  const uint64_t offsetBytes = DecodeBlockchainWordAsUint64(words[index]);
  const size_t offsetWords = (size_t)(offsetBytes / 32);
  if ((offsetBytes % 32) != 0 || offsetWords >= words.size())
  {
    return {};
  }

  const uint64_t length = DecodeBlockchainWordAsUint64(words[offsetWords]);
  const size_t dataStart = offsetWords + 1;
  const size_t dataWordCount = (size_t)((length + 31) / 32);
  if (dataStart + dataWordCount > words.size())
  {
    return {};
  }

  std::string hexBytes = {};
  for (size_t i = 0; i < dataWordCount; ++i)
  {
    hexBytes += words[dataStart + i];
  }
  hexBytes = hexBytes.substr(0, (size_t)length * 2);
  return HexToBytes(hexBytes);
}
