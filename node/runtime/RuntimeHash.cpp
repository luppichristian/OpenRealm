#include "RuntimeHash.h"

#include <cstdio>

static constexpr uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037ull;
static constexpr uint64_t FNV_PRIME_64 = 1099511628211ull;

uint64_t ComputeHash64(const std::vector<uint8_t>& bytes)
{
  uint64_t hash = FNV_OFFSET_BASIS_64;
  for (uint8_t byte : bytes)
  {
    hash ^= (uint64_t)byte;
    hash *= FNV_PRIME_64;
  }
  return hash;
}

uint64_t ComputeHash64(const std::string& text)
{
  std::vector<uint8_t> bytes = {};
  bytes.reserve(text.size());
  for (char ch : text)
  {
    bytes.push_back((uint8_t)ch);
  }
  return ComputeHash64(bytes);
}

std::string FormatHash64(uint64_t value)
{
  char buffer[32] = {};
  std::snprintf(buffer, sizeof(buffer), "0x%016llx", (unsigned long long)value);
  return std::string(buffer);
}
