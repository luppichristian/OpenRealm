#include <cstdint>
#include <iostream>
#include <vector>

#include "../blockchain/Blockchain.h"
#include "../blockchain/BlockchainConfig.h"
#include "../blockchain/Wallet.h"
#include "../runtime/RuntimeNetClient.h"
#include "../runtime/RuntimePacket.h"

int main()
{
  RuntimeNetClient runtimeNet = {};
  RuntimePeerAddress relayBindAddress = {"127.0.0.1", 46000};
  RuntimePeerAddress relayPeerAddress = {"127.0.0.1", 46001};

  RuntimePacket handshakePacket = MakeHandshakePacket(1, 42);
  std::vector<uint8_t> handshakeBytes = SerializeRuntimePacket(handshakePacket);
  RuntimePacket parsedPacket = {};

  const bool started = runtimeNet.Start(relayBindAddress);
  const bool parsed = TryParseRuntimePacket(handshakeBytes, &parsedPacket);
  const bool sentPacket = started && runtimeNet.SendPacket(relayPeerAddress, handshakeBytes);

  BlockchainConfig blockchainConfig = {};
  blockchainConfig.rpcUrl = "http://127.0.0.1:8545";
  blockchainConfig.globalParamsAddress = "0x0000000000000000000000000000000000000000";
  blockchainConfig.playerRegistryAddress = "0x0000000000000000000000000000000000000000";
  blockchainConfig.chunkClaimsAddress = "0x0000000000000000000000000000000000000000";
  blockchainConfig.marketplaceAddress = "0x0000000000000000000000000000000000000000";

  WalletState walletState = {};
  walletState.connected = true;
  walletState.accountAddress = "0x0000000000000000000000000000000000000001";
  walletState.runtimeSignerAddress = "0x0000000000000000000000000000000000000001";

  Blockchain blockchain(blockchainConfig, Wallet(walletState));

  const std::string chainId = blockchain.GetRpcClient().EthChainId();
  GlobalParamsState globalParamsState = blockchain.GetGlobalParams().GetState();
  ResolvedRuntimeAccount runtimeAccount = blockchain.GetPlayerRegistry().ResolveRuntimeAccount(
      blockchain.GetWallet().GetActiveSignerAddress());
  ChunkRuntimeState chunkRuntimeState = blockchain.GetChunkClaims().GetChunkRuntimeState(
      0,
      0,
      blockchain.GetWallet().GetActiveSignerAddress());
  SaleState saleState = blockchain.GetMarketplace().GetSaleStateForChunk(0, 0);

  std::cout << "OpenRealm relay stack\n";
  std::cout << "- runtime networking: " << runtimeNet.DescribeStack() << "\n";
  std::cout << "- relay client startup: " << (started ? "ok" : "failed") << "\n";
  std::cout << "- runtime packet type: " << DescribeRuntimePacketType(parsedPacket.type) << "\n";
  std::cout << "- runtime packet bytes: " << handshakeBytes.size() << "\n";
  std::cout << "- runtime packet parse: " << (parsed ? "ok" : "failed") << "\n";
  std::cout << "- relay packet send: " << (sentPacket ? "ok" : "failed") << "\n";
  std::cout << "- blockchain backend: " << blockchain.GetRpcClient().DescribeStack() << "\n";
  std::cout << "- blockchain facade: " << blockchain.DescribeStack() << "\n";
  std::cout << "- blockchain rpc url: " << blockchain.GetRpcClient().GetRpcUrl() << "\n";
  std::cout << "- wallet state: " << blockchain.GetWallet().DescribeWallet() << "\n";
  std::cout << "- wallet account: " << blockchain.GetWallet().GetAccountAddress() << "\n";
  std::cout << "- wallet active signer: " << blockchain.GetWallet().GetActiveSignerAddress() << "\n";
  std::cout << "- eth_chainId probe: " << (chainId.empty() ? "unavailable" : chainId) << "\n";
  std::cout << "- global params wrapper: " << blockchain.GetGlobalParams().DescribeContract() << " @ " << blockchain.GetGlobalParams().GetContractAddress() << "\n";
  std::cout << "- player registry wrapper: " << blockchain.GetPlayerRegistry().DescribeContract() << " @ " << blockchain.GetPlayerRegistry().GetContractAddress() << "\n";
  std::cout << "- chunk claims wrapper: " << blockchain.GetChunkClaims().DescribeContract() << " @ " << blockchain.GetChunkClaims().GetContractAddress() << "\n";
  std::cout << "- marketplace wrapper: " << blockchain.GetMarketplace().DescribeContract() << " @ " << blockchain.GetMarketplace().GetContractAddress() << "\n";
  std::cout << "- global params available: " << (globalParamsState.available ? "yes" : "no") << "\n";
  std::cout << "- runtime account resolved: " << (runtimeAccount.available ? "yes" : "no") << "\n";
  std::cout << "- chunk runtime state available: " << (chunkRuntimeState.available ? "yes" : "no") << "\n";
  std::cout << "- sale state available: " << (saleState.available ? "yes" : "no") << "\n";

  return (started && parsed) ? 0 : 1;
}
