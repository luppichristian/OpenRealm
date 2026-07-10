#include "RuntimeAuth.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <nlohmann/json.hpp>

#ifdef _WIN32
#define OPENREALM_POPEN _popen
#define OPENREALM_PCLOSE _pclose
#else
#define OPENREALM_POPEN popen
#define OPENREALM_PCLOSE pclose
#endif

namespace
{
std::filesystem::path FindRuntimeWalletAuthScript()
{
  std::filesystem::path current = std::filesystem::current_path();
  while (true)
  {
    const std::filesystem::path candidate = current / "blockchain" / "scripts" / "runtimeWalletAuth.js";
    if (std::filesystem::exists(candidate))
    {
      return candidate;
    }

    if (!current.has_parent_path() || current.parent_path() == current)
    {
      break;
    }
    current = current.parent_path();
  }

  return {};
}

std::filesystem::path WriteRuntimeWalletAuthPayload(const nlohmann::json& payload)
{
  static std::atomic<uint64_t> counter{0};
#if defined(_WIN32)
  const unsigned long processId = (unsigned long)GetCurrentProcessId();
#else
  const unsigned long processId = (unsigned long)getpid();
#endif

  for (int attempt = 0; attempt < 16; ++attempt)
  {
    const auto uniqueId = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const uint64_t serial = counter.fetch_add(1, std::memory_order_relaxed);
    const std::filesystem::path payloadPath = std::filesystem::temp_directory_path()
        / ("openrealm-runtime-auth-" + std::to_string(processId) + "-" + std::to_string(serial) + "-" + std::to_string(uniqueId) + ".json");
    if (std::filesystem::exists(payloadPath)) continue;

    std::ofstream stream(payloadPath, std::ios::out | std::ios::trunc);
    if (!stream.is_open()) continue;
    stream << payload.dump();
    stream.close();
    return payloadPath;
  }

  return {};
}

std::string QuoteShellPath(const std::filesystem::path& path)
{
  return "\"" + path.generic_string() + "\"";
}

bool RunRuntimeWalletAuthScript(const nlohmann::json& payload, nlohmann::json* response)
{
  if (response == nullptr) return false;

  const std::filesystem::path scriptPath = FindRuntimeWalletAuthScript();
  if (scriptPath.empty()) return false;

  const std::filesystem::path payloadPath = WriteRuntimeWalletAuthPayload(payload);
  if (payloadPath.empty()) return false;

  const std::string command = "node " + QuoteShellPath(scriptPath) + " " + QuoteShellPath(payloadPath) + " 2>&1";

  std::string output = {};
  FILE* pipe = OPENREALM_POPEN(command.c_str(), "r");
  if (pipe == nullptr)
  {
    std::error_code removeError = {};
    std::filesystem::remove(payloadPath, removeError);
    return false;
  }

  char buffer[512] = {};
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
  {
    output += buffer;
  }
  const int exitCode = OPENREALM_PCLOSE(pipe);

  std::error_code removeError = {};
  std::filesystem::remove(payloadPath, removeError);
  if (exitCode != 0) return false;

  *response = nlohmann::json::parse(output, nullptr, false);
  return !response->is_discarded() && response->value("ok", false);
}

std::string BytesToHex(const std::vector<uint8_t>& bytes)
{
  static constexpr char kHex[] = "0123456789abcdef";
  std::string hex = "0x";
  hex.reserve(2 + bytes.size() * 2);
  for (uint8_t byte : bytes)
  {
    hex.push_back(kHex[(byte >> 4) & 0x0f]);
    hex.push_back(kHex[byte & 0x0f]);
  }
  return hex;
}
} // namespace

std::string NormalizeRuntimeAuthAddress(const std::string& address)
{
  std::string normalized = address;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c)
                 { return (char)std::tolower(c); });
  return normalized;
}

bool ResolveRuntimeWalletAddress(const Wallet& wallet, std::string* signerAddress)
{
  if (signerAddress == nullptr || !wallet.HasPrivateKey()) return false;

  nlohmann::json payload = {
      {"mode", "address"},
      {"privateKey", wallet.GetPrivateKey()},
  };
  nlohmann::json response = {};
  if (!RunRuntimeWalletAuthScript(payload, &response)) return false;

  *signerAddress = NormalizeRuntimeAuthAddress(response.value("address", std::string {}));
  return !signerAddress->empty();
}

bool SignRuntimeAuthMessage(const Wallet& wallet, const std::vector<uint8_t>& messageBytes, std::string* signerAddress, std::string* signatureHex)
{
  if (signerAddress == nullptr || signatureHex == nullptr || !wallet.HasPrivateKey() || messageBytes.empty()) return false;

  nlohmann::json payload = {
      {"mode", "sign"},
      {"privateKey", wallet.GetPrivateKey()},
      {"messageHex", BytesToHex(messageBytes)},
  };
  nlohmann::json response = {};
  if (!RunRuntimeWalletAuthScript(payload, &response)) return false;

  *signerAddress = NormalizeRuntimeAuthAddress(response.value("address", std::string {}));
  *signatureHex = response.value("signature", std::string {});
  return !signerAddress->empty() && !signatureHex->empty();
}

bool RecoverRuntimeAuthSigner(const std::vector<uint8_t>& messageBytes, const std::string& signatureHex, std::string* signerAddress)
{
  if (signerAddress == nullptr || signatureHex.empty() || messageBytes.empty()) return false;

  nlohmann::json payload = {
      {"mode", "recover"},
      {"messageHex", BytesToHex(messageBytes)},
      {"signature", signatureHex},
  };
  nlohmann::json response = {};
  if (!RunRuntimeWalletAuthScript(payload, &response)) return false;

  *signerAddress = NormalizeRuntimeAuthAddress(response.value("address", std::string {}));
  return !signerAddress->empty();
}
