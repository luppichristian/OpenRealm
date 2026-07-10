#include "BlockchainRpcClient.h"

#include "BlockchainAbi.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <utility>

#include <httplib.h>
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
std::string StripHttpScheme(const std::string& url)
{
  const std::string prefix = "http://";
  if (url.rfind(prefix, 0) == 0)
  {
    return url.substr(prefix.size());
  }
  return url;
}

bool PostJsonRpc(const BlockchainConfig& config, const nlohmann::json& payload, nlohmann::json* result)
{
  if (result == nullptr || config.rpcUrl.empty())
  {
    return false;
  }

  httplib::Client client(StripHttpScheme(config.rpcUrl));
  client.set_connection_timeout(config.connectionTimeoutSeconds, 0);
  client.set_read_timeout(config.readTimeoutSeconds, 0);
  client.set_write_timeout(config.writeTimeoutSeconds, 0);

  httplib::Result response = client.Post("/", payload.dump(), "application/json");
  if (!response || response->status < 200 || response->status >= 300)
  {
    return false;
  }

  *result = nlohmann::json::parse(response->body, nullptr, false);
  if (result->is_discarded() || result->contains("error"))
  {
    return false;
  }
  return true;
}

std::filesystem::path FindNativeSendScript()
{
  std::filesystem::path current = std::filesystem::current_path();
  while (true)
  {
    const std::filesystem::path candidate = current / "blockchain" / "scripts" / "nativeSendTransaction.js";
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

std::filesystem::path WriteNativeSignerPayload(const nlohmann::json& payload)
{
  const auto uniqueId = std::chrono::high_resolution_clock::now().time_since_epoch().count();
  const std::filesystem::path payloadPath =
      std::filesystem::temp_directory_path() / ("openrealm-native-send-" + std::to_string(uniqueId) + ".json");
  std::ofstream stream(payloadPath, std::ios::out | std::ios::trunc);
  if (!stream.is_open())
  {
    return {};
  }

  stream << payload.dump();
  stream.close();
  return payloadPath;
}

std::string QuoteShellPath(const std::filesystem::path& path)
{
  return "\"" + path.generic_string() + "\"";
}

bool RunNativeSendScript(
    const BlockchainConfig& config,
    const Wallet& wallet,
    const std::string& contractAddress,
    const std::string& callData,
    const std::string& valueHex,
    std::string* transactionHash)
{
  if (transactionHash == nullptr || !wallet.HasPrivateKey() || IsZeroBlockchainAddress(contractAddress) || callData.empty())
  {
    return false;
  }

  const std::filesystem::path scriptPath = FindNativeSendScript();
  if (scriptPath.empty())
  {
    return false;
  }

  nlohmann::json payload = {
      {    "rpcUrl",                                    config.rpcUrl},
      {"privateKey",                              wallet.GetPrivateKey()},
      {        "to", NormalizeBlockchainAddress(contractAddress)},
      {      "data",                                     callData}
  };
  if (!valueHex.empty())
  {
    payload["value"] = NormalizeBlockchainUintHex(valueHex);
  }

  const std::filesystem::path payloadPath = WriteNativeSignerPayload(payload);
  if (payloadPath.empty())
  {
    return false;
  }

  const std::string command =
      "node " + QuoteShellPath(scriptPath) + " " + QuoteShellPath(payloadPath) + " 2>&1";

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
  if (exitCode != 0)
  {
    return false;
  }

  const nlohmann::json response = nlohmann::json::parse(output, nullptr, false);
  if (response.is_discarded() || !response.value("ok", false))
  {
    return false;
  }

  *transactionHash = response.value("hash", std::string {});
  return !transactionHash->empty();
}

void ParseReceiptLogs(const nlohmann::json& receiptJson, BlockchainTransactionReceipt* receipt)
{
  receipt->logs.clear();
  if (!receiptJson.contains("logs") || !receiptJson["logs"].is_array())
  {
    return;
  }

  for (const nlohmann::json& entry : receiptJson["logs"])
  {
    if (!entry.is_object())
    {
      continue;
    }

    BlockchainTransactionLog log = {};
    log.address = entry.value("address", std::string {});
    log.data = entry.value("data", std::string {});
    if (entry.contains("topics") && entry["topics"].is_array())
    {
      for (const nlohmann::json& topic : entry["topics"])
      {
        if (topic.is_string())
        {
          log.topics.push_back(topic.get<std::string>());
        }
      }
    }

    receipt->logs.push_back(std::move(log));
  }
}
}

BlockchainRpcClient::BlockchainRpcClient(BlockchainConfig blockchainConfig)
    : config(std::move(blockchainConfig))
{
}

const BlockchainConfig& BlockchainRpcClient::GetConfig() const
{
  return config;
}

const std::string& BlockchainRpcClient::GetRpcUrl() const
{
  return config.rpcUrl;
}

std::string BlockchainRpcClient::DescribeStack() const
{
  return "cpp-httplib + nlohmann/json JSON-RPC client";
}

std::string BlockchainRpcClient::EthChainId() const
{
  const nlohmann::json payload = {
      {"jsonrpc",                   "2.0"},
      {     "id",                       1},
      { "method",           "eth_chainId"},
      { "params", nlohmann::json::array()}
  };

  nlohmann::json response = {};
  if (!PostJsonRpc(config, payload, &response))
  {
    return {};
  }
  if (!response.contains("result") || !response["result"].is_string())
  {
    return {};
  }

  return response["result"].get<std::string>();
}

bool BlockchainRpcClient::EthCall(
    const std::string& contractAddress,
    const std::string& callData,
    std::string* resultHex,
    const std::string& fromAddress,
    const std::string& valueHex) const
{
  if (resultHex == nullptr || IsZeroBlockchainAddress(contractAddress) || callData.empty())
  {
    return false;
  }

  nlohmann::json callObject = {
      {  "to", NormalizeBlockchainAddress(contractAddress)},
      {"data",                                    callData}
  };
  if (!fromAddress.empty() && !IsZeroBlockchainAddress(fromAddress))
  {
    callObject["from"] = NormalizeBlockchainAddress(fromAddress);
  }
  if (!valueHex.empty())
  {
    callObject["value"] = NormalizeBlockchainUintHex(valueHex);
  }

  const nlohmann::json payload = {
      {"jsonrpc",                                         "2.0"},
      {     "id",                                             1},
      { "method",                                    "eth_call"},
      { "params", nlohmann::json::array({callObject, "latest"})}
  };

  nlohmann::json response = {};
  if (!PostJsonRpc(config, payload, &response))
  {
    return false;
  }
  if (!response.contains("result") || !response["result"].is_string())
  {
    return false;
  }

  *resultHex = response["result"].get<std::string>();
  return true;
}

bool BlockchainRpcClient::EthSendTransaction(
    const std::string& fromAddress,
    const std::string& contractAddress,
    const std::string& callData,
    std::string* transactionHash,
    const std::string& valueHex) const
{
  if (
      transactionHash == nullptr || IsZeroBlockchainAddress(fromAddress) || IsZeroBlockchainAddress(contractAddress) ||
      callData.empty())
  {
    return false;
  }

  nlohmann::json txObject = {
      {"from",     NormalizeBlockchainAddress(fromAddress)},
      {  "to", NormalizeBlockchainAddress(contractAddress)},
      {"data",                                    callData}
  };
  if (!valueHex.empty())
  {
    txObject["value"] = NormalizeBlockchainUintHex(valueHex);
  }

  const nlohmann::json payload = {
      {"jsonrpc",                             "2.0"},
      {     "id",                                 1},
      { "method",             "eth_sendTransaction"},
      { "params", nlohmann::json::array({txObject})}
  };

  nlohmann::json response = {};
  if (!PostJsonRpc(config, payload, &response))
  {
    return false;
  }
  if (!response.contains("result") || !response["result"].is_string())
  {
    return false;
  }

  *transactionHash = response["result"].get<std::string>();
  return !transactionHash->empty();
}

bool BlockchainRpcClient::EthSendTransaction(
    const Wallet& wallet,
    const std::string& contractAddress,
    const std::string& callData,
    std::string* transactionHash,
    const std::string& valueHex) const
{
  if (wallet.HasPrivateKey())
  {
    return RunNativeSendScript(config, wallet, contractAddress, callData, valueHex, transactionHash);
  }
  if (!wallet.HasAccountAddress())
  {
    return false;
  }

  return EthSendTransaction(wallet.GetAccountAddress(), contractAddress, callData, transactionHash, valueHex);
}

bool BlockchainRpcClient::EthGetTransactionReceipt(const std::string& transactionHash, BlockchainTransactionReceipt* receipt) const
{
  if (receipt == nullptr || transactionHash.empty())
  {
    return false;
  }

  const nlohmann::json payload = {
      {"jsonrpc",                                    "2.0"},
      {     "id",                                        1},
      { "method",              "eth_getTransactionReceipt"},
      { "params", nlohmann::json::array({transactionHash})}
  };

  nlohmann::json response = {};
  if (!PostJsonRpc(config, payload, &response))
  {
    return false;
  }
  if (!response.contains("result"))
  {
    return false;
  }
  if (response["result"].is_null())
  {
    *receipt = {};
    return true;
  }
  if (!response["result"].is_object())
  {
    return false;
  }

  const nlohmann::json& result = response["result"];
  receipt->available = true;
  receipt->transactionHash = result.value("transactionHash", std::string {});
  receipt->blockNumber = result.value("blockNumber", std::string {});
  receipt->gasUsed = result.value("gasUsed", std::string {});
  receipt->status = result.value("status", std::string {});
  receipt->contractAddress = result.value("contractAddress", std::string {});
  receipt->success = NormalizeBlockchainUintHex(receipt->status) == "0x1";
  ParseReceiptLogs(result, receipt);
  return true;
}

bool BlockchainRpcClient::WaitForTransactionReceipt(
    const std::string& transactionHash,
    BlockchainTransactionReceipt* receipt,
    int timeoutMilliseconds,
    int pollMilliseconds) const
{
  if (receipt == nullptr || transactionHash.empty())
  {
    return false;
  }

  const auto startedAt = std::chrono::steady_clock::now();
  while (true)
  {
    BlockchainTransactionReceipt polledReceipt = {};
    if (!EthGetTransactionReceipt(transactionHash, &polledReceipt))
    {
      return false;
    }
    if (polledReceipt.available)
    {
      *receipt = polledReceipt;
      return true;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt);
    if (elapsed.count() >= timeoutMilliseconds)
    {
      return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(pollMilliseconds));
  }
}
