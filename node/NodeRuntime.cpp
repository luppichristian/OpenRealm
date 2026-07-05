#include "NodeRuntime.h"

#include <cstring>

NodeBootConfig ParseNodeBootConfig(int argc, char** argv)
{
  NodeBootConfig config = {};

  for (int i = 1; i < argc; ++i)
  {
    if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)
    {
      config.configPath = argv[++i];
      continue;
    }

    if (std::strcmp(argv[i], "--realm-dir") == 0 && i + 1 < argc)
    {
      config.realmDirectory = argv[++i];
      continue;
    }
  }

  return config;
}

RuntimeRealmState BuildNodeRuntimeRealmState(Blockchain& blockchain, const BlockchainConfig& blockchainConfig)
{
  RuntimeRealmState realmState = {};
  const std::string chainId = blockchain.GetRpcClient().EthChainId();
  realmState.chainId = chainId.empty() ? "unavailable" : chainId;
  realmState.blockchainConfig = blockchainConfig;
  realmState.globalParams = blockchain.GetGlobalParams().GetState();
  return realmState;
}
