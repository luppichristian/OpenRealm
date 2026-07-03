#pragma once

#include <cstdint>
#include <string>
#include <vector>

uint64_t ComputeHash64(const std::vector<uint8_t>& bytes);
uint64_t ComputeHash64(const std::string& text);
std::string FormatHash64(uint64_t value);
