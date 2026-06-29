#include "BlockchainRpcClient.h"

#include <utility>

#include <httplib.h>
#include <nlohmann/json.hpp>

static std::string StripHttpScheme(const std::string& url)
{
  const std::string prefix = "http://";
  if (url.rfind(prefix, 0) == 0) return url.substr(prefix.size());
  return url;
}

BlockchainRpcClient::BlockchainRpcClient(std::string url)
    : rpcUrl(std::move(url))
{
}

const std::string& BlockchainRpcClient::GetRpcUrl() const
{
  return rpcUrl;
}

std::string BlockchainRpcClient::DescribeStack() const
{
  return "cpp-httplib + nlohmann/json JSON-RPC client";
}

std::string BlockchainRpcClient::EthChainId() const
{
  if (rpcUrl.empty()) return {};

  httplib::Client client(StripHttpScheme(rpcUrl));
  client.set_connection_timeout(1, 0);
  client.set_read_timeout(1, 0);
  client.set_write_timeout(1, 0);

  const nlohmann::json payload = {
      {"jsonrpc",                   "2.0"},
      {     "id",                       1},
      { "method",           "eth_chainId"},
      { "params", nlohmann::json::array()}
  };

  httplib::Result response = client.Post("/", payload.dump(), "application/json");
  if (!response || response->status < 200 || response->status >= 300) return {};

  const nlohmann::json json = nlohmann::json::parse(response->body, nullptr, false);
  if (json.is_discarded() || !json.contains("result") || !json["result"].is_string()) return {};

  return json["result"].get<std::string>();
}
