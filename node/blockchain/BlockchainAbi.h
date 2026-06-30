#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct BlockchainAbiArgument
{
  bool dynamic = false;
  std::string headWord = {};
  std::vector<std::string> tailWords = {};
};

BlockchainAbiArgument EncodeBlockchainAddressArgument(const std::string& address);
BlockchainAbiArgument EncodeBlockchainBoolArgument(bool value);
BlockchainAbiArgument EncodeBlockchainInt32Argument(int32_t value);
BlockchainAbiArgument EncodeBlockchainUint64Argument(uint64_t value);
BlockchainAbiArgument EncodeBlockchainUint96Argument(uint64_t value);
BlockchainAbiArgument EncodeBlockchainUint256Argument(const std::string& value);
BlockchainAbiArgument EncodeBlockchainBytes32Argument(const std::string& value);
BlockchainAbiArgument EncodeBlockchainStringArgument(const std::string& value);
std::string BuildBlockchainCallData(const std::string& selector, const std::vector<BlockchainAbiArgument>& encodedArguments);
std::vector<std::string> SplitBlockchainWords(const std::string& encodedData);
std::string DecodeBlockchainWordAsAddress(const std::string& word);
bool DecodeBlockchainWordAsBool(const std::string& word);
int32_t DecodeBlockchainWordAsInt32(const std::string& word);
uint64_t DecodeBlockchainWordAsUint64(const std::string& word);
std::string DecodeBlockchainWordAsUintHex(const std::string& word);
std::string DecodeBlockchainWordAsBytes32Hex(const std::string& word);
std::string DecodeBlockchainStringAt(const std::vector<std::string>& words, size_t index);
std::string NormalizeBlockchainAddress(const std::string& address);
std::string NormalizeBlockchainBytes32(const std::string& value);
std::string NormalizeBlockchainUintHex(const std::string& value);
bool IsZeroBlockchainAddress(const std::string& address);
