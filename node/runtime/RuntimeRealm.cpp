#include "RuntimeRealm.h"

#include "RuntimeHash.h"

#include <sstream>

std::string BuildRuntimeRealmFingerprint(const RuntimeRealmState& realmState)
{
  std::ostringstream stream = {};
  stream << "runtimeProtocolVersion=" << realmState.runtimeProtocolVersion << '\n';
  stream << "chainId=" << realmState.chainId << '\n';
  stream << "blockchainProtocolVersion=" << realmState.blockchainConfig.protocolVersion << '\n';
  stream << "rpcUrl=" << realmState.blockchainConfig.rpcUrl << '\n';
  stream << "globalParamsAddress=" << realmState.blockchainConfig.globalParamsAddress << '\n';
  stream << "playerRegistryAddress=" << realmState.blockchainConfig.playerRegistryAddress << '\n';
  stream << "chunkClaimsAddress=" << realmState.blockchainConfig.chunkClaimsAddress << '\n';
  stream << "marketplaceAddress=" << realmState.blockchainConfig.marketplaceAddress << '\n';
  stream << "connectionTimeoutSeconds=" << realmState.blockchainConfig.connectionTimeoutSeconds << '\n';
  stream << "readTimeoutSeconds=" << realmState.blockchainConfig.readTimeoutSeconds << '\n';
  stream << "writeTimeoutSeconds=" << realmState.blockchainConfig.writeTimeoutSeconds << '\n';
  stream << "globalParamsAvailable=" << (realmState.globalParams.available ? 1 : 0) << '\n';
  stream << "minChunkCoord=" << realmState.globalParams.minChunkCoord << '\n';
  stream << "maxChunkCoord=" << realmState.globalParams.maxChunkCoord << '\n';
  stream << "minChunkPrice=" << realmState.globalParams.minChunkPrice << '\n';
  stream << "maxFeeBps=" << realmState.globalParams.maxFeeBps << '\n';
  stream << "minAuctionDuration=" << realmState.globalParams.minAuctionDuration << '\n';
  return stream.str();
}

uint64_t ComputeRuntimeRealmHash(const RuntimeRealmState& realmState)
{
  return ComputeHash64(BuildRuntimeRealmFingerprint(realmState));
}
