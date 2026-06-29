#include "BlockchainRpcClient.h"

#include "BlockchainAbi.h"

#include <utility>

#include <httplib.h>
#include <nlohmann/json.hpp>

static std::string StripHttpScheme(const std::string& url)
{
  const std::string prefix = "http://";
  if (url.rfind(prefix, 0) == 0) return url.substr(prefix.size());
  return url;
}

static bool PostJsonRpc(const BlockchainConfig& config, const nlohmann::json& payload, nlohmann::json* result)
{
  if (result == nullptr || config.rpcUrl.empty()) return false;

  httplib::Client client(StripHttpScheme(config.rpcUrl));
  client.set_connection_timeout(config.connectionTimeoutSeconds, 0);
  client.set_read_timeout(config.readTimeoutSeconds, 0);
  client.set_write_timeout(config.writeTimeoutSeconds, 0);

  httplib::Result response = client.Post("/", payload.dump(), "application/json");
  if (!response || response->status < 200 || response->status >= 300) return false;

  *result = nlohmann::json::parse(response->body, nullptr, false);
  if (result->is_discarded() || result->contains("error")) return false;
  return true;
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
  if (!PostJsonRpc(config, payload, &response)) return {};
  if (!response.contains("result") || !response["result"].is_string()) return {};

  return response["result"].get<std::string>();
}

bool BlockchainRpcClient::EthCall(const std::string& contractAddress, const std::string& callData, std::string* resultHex) const
{
  if (resultHex == nullptr || IsZeroBlockchainAddress(contractAddress) || callData.empty()) return false;

  const nlohmann::json payload = {
      {"jsonrpc",                                                           "2.0"},
      {     "id",                                                               1},
      { "method",                                                      "eth_call"},
      { "params",
        nlohmann::json::array({
          {
              {  "to", NormalizeBlockchainAddress(contractAddress)},
              {"data",                           callData        }
          },
          "latest"
        })}
  };

  nlohmann::json response = {};
  if (!PostJsonRpc(config, payload, &response)) return false;
  if (!response.contains("result") || !response["result"].is_string()) return false;

  *resultHex = response["result"].get<std::string>();
  return true;
}
