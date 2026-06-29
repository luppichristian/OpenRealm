#pragma once

#include <cstdint>
#include <string>
#include <vector>

std::string EncodeBlockchainAddressArgument(const std::string& address);
std::string EncodeBlockchainInt32Argument(int32_t value);
std::string EncodeBlockchainUint64Argument(uint64_t value);
std::string BuildBlockchainCallData(const std::string& selector, const std::vector<std::string>& encodedArguments);
std::vector<std::string> SplitBlockchainWords(const std::string& encodedData);
std::string DecodeBlockchainWordAsAddress(const std::string& word);
bool DecodeBlockchainWordAsBool(const std::string& word);
int32_t DecodeBlockchainWordAsInt32(const std::string& word);
uint64_t DecodeBlockchainWordAsUint64(const std::string& word);
std::string DecodeBlockchainWordAsUintHex(const std::string& word);
std::string NormalizeBlockchainAddress(const std::string& address);
bool IsZeroBlockchainAddress(const std::string& address);
