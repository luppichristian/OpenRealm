#include <cstdint>
#include <iostream>
#include <vector>

#include "../blockchain/BlockchainRpcClient.h"
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

  BlockchainRpcClient blockchain("http://127.0.0.1:8545");
  const std::string chainId = blockchain.EthChainId();

  std::cout << "OpenRealm relay stack\n";
  std::cout << "- runtime networking: " << runtimeNet.DescribeStack() << "\n";
  std::cout << "- relay client startup: " << (started ? "ok" : "failed") << "\n";
  std::cout << "- runtime packet type: " << DescribeRuntimePacketType(parsedPacket.type) << "\n";
  std::cout << "- runtime packet bytes: " << handshakeBytes.size() << "\n";
  std::cout << "- runtime packet parse: " << (parsed ? "ok" : "failed") << "\n";
  std::cout << "- relay packet send: " << (sentPacket ? "ok" : "failed") << "\n";
  std::cout << "- blockchain backend: " << blockchain.DescribeStack() << "\n";
  std::cout << "- eth_chainId probe: " << (chainId.empty() ? "unavailable" : chainId) << "\n";

  return (started && parsed) ? 0 : 1;
}
