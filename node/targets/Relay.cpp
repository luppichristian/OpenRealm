#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

#include "../blockchain/Blockchain.h"
#include "../blockchain/BlockchainConfig.h"
#include "../blockchain/Wallet.h"
#include "../runtime/ActiveNodeBucket.h"
#include "../runtime/Packet.h"
#include "../runtime/PacketValidator.h"
#include "../runtime/RuntimeClient.h"
#include "../runtime/RuntimeHash.h"
#include "../runtime/RuntimeRealm.h"

struct ReceivedPacketState
{
  bool received = false;
  RuntimePeerAddress peerAddress = {};
  PacketValidationResult validation = {};
};

static ReceivedPacketState ReceiveAndValidate(
    RuntimeClient& listener,
    const PacketValidationContext& validationContext,
    uint32_t timeoutMs
)
{
  ReceivedPacketState state = {};
  std::vector<uint8_t> bytes = {};
  if (!listener.ReceivePacket(&bytes, &state.peerAddress, timeoutMs)) return state;

  state.received = true;
  state.validation = ValidateIncomingPacket(bytes, state.peerAddress, validationContext);
  return state;
}

int main()
{
  RuntimePeerAddress relayBindAddress = {"127.0.0.1", 46001};
  RuntimePeerAddress acceptedPeerBindAddress = {"127.0.0.1", 46000};
  RuntimePeerAddress duplicatePeerBindAddress = {"127.0.0.1", 46002};
  RuntimePeerAddress mismatchedRealmPeerBindAddress = {"127.0.0.1", 46003};

  RuntimeClient relayListener = {};
  RuntimeClient acceptedPeer = {};
  RuntimeClient duplicatePeer = {};
  RuntimeClient mismatchedRealmPeer = {};

  const bool relayStarted = relayListener.Start(relayBindAddress);
  const bool acceptedPeerStarted = acceptedPeer.Start(acceptedPeerBindAddress);
  const bool duplicatePeerStarted = duplicatePeer.Start(duplicatePeerBindAddress);
  const bool mismatchedRealmPeerStarted = mismatchedRealmPeer.Start(mismatchedRealmPeerBindAddress);

  BlockchainConfig blockchainConfig = {};
  blockchainConfig.rpcUrl = "http://127.0.0.1:8545";
  blockchainConfig.globalParamsAddress = "0x0000000000000000000000000000000000000000";
  blockchainConfig.playerRegistryAddress = "0x0000000000000000000000000000000000000000";
  blockchainConfig.chunkClaimsAddress = "0x0000000000000000000000000000000000000000";
  blockchainConfig.marketplaceAddress = "0x0000000000000000000000000000000000000000";

  Wallet wallet("0x0000000000000000000000000000000000000001",
                "0x0000000000000000000000000000000000000001");
  Blockchain blockchain(blockchainConfig, std::move(wallet));

  const std::string chainId = blockchain.GetRpcClient().EthChainId();
  GlobalParamsState globalParamsState = blockchain.GetGlobalParams().GetState();
  ResolvedRuntimeAccount runtimeAccount = blockchain.GetPlayerRegistry().ResolveRuntimeAccount(
      blockchain.GetWallet().GetActiveSignerAddress());
  ChunkRuntimeState chunkRuntimeState = blockchain.GetChunkClaims().GetChunkRuntimeState(
      0,
      0,
      blockchain.GetWallet().GetActiveSignerAddress());
  SaleState saleState = blockchain.GetMarketplace().GetSaleStateForChunk(0, 0);

  RuntimeRealmState realmState = {};
  realmState.chainId = chainId.empty() ? "unavailable" : chainId;
  realmState.blockchainConfig = blockchainConfig;
  realmState.globalParams = globalParamsState;

  const uint64_t realmHash = ComputeRuntimeRealmHash(realmState);
  ActiveNodeBucket activeNodes = {};
  PacketValidationContext validationContext = {};
  validationContext.localNodeId = 9001;
  validationContext.expectedRealmHash = realmHash;
  validationContext.activeNodes = &activeNodes;

  HandshakePacketData acceptedHandshake = {};
  acceptedHandshake.protocolVersion = 1;
  acceptedHandshake.nodeId = 42;
  acceptedHandshake.realmHash = realmHash;

  HandshakePacketData duplicateHandshake = acceptedHandshake;

  HandshakePacketData mismatchedRealmHandshake = {};
  mismatchedRealmHandshake.protocolVersion = 1;
  mismatchedRealmHandshake.nodeId = 84;
  mismatchedRealmHandshake.realmHash = realmHash + 1;

  const std::vector<uint8_t> acceptedBytes = SerializePacket(MakeHandshakePacket(acceptedHandshake));
  const std::vector<uint8_t> duplicateBytes = SerializePacket(MakeHandshakePacket(duplicateHandshake));
  const std::vector<uint8_t> mismatchedRealmBytes = SerializePacket(MakeHandshakePacket(mismatchedRealmHandshake));

  const bool sentAcceptedPacket = acceptedPeerStarted && acceptedPeer.SendPacket(relayBindAddress, acceptedBytes);
  ReceivedPacketState acceptedPacket = ReceiveAndValidate(relayListener, validationContext, 1000);

  const bool sentDuplicatePacket = duplicatePeerStarted && duplicatePeer.SendPacket(relayBindAddress, duplicateBytes);
  ReceivedPacketState duplicatePacket = ReceiveAndValidate(relayListener, validationContext, 1000);

  const bool sentMismatchedRealmPacket = mismatchedRealmPeerStarted
      && mismatchedRealmPeer.SendPacket(relayBindAddress, mismatchedRealmBytes);
  ReceivedPacketState mismatchedRealmPacket = ReceiveAndValidate(relayListener, validationContext, 1000);

  std::cout << "OpenRealm relay stack\n";
  std::cout << "- runtime networking: " << relayListener.DescribeStack() << "\n";
  std::cout << "- relay listener startup: " << (relayStarted ? "ok" : "failed") << "\n";
  std::cout << "- accepted peer startup: " << (acceptedPeerStarted ? "ok" : "failed") << "\n";
  std::cout << "- duplicate peer startup: " << (duplicatePeerStarted ? "ok" : "failed") << "\n";
  std::cout << "- mismatched realm peer startup: " << (mismatchedRealmPeerStarted ? "ok" : "failed") << "\n";
  std::cout << "- runtime accepted packet bytes: " << acceptedBytes.size() << "\n";
  std::cout << "- runtime accepted packet checksum: " << acceptedPacket.validation.packetChecksum << "\n";
  std::cout << "- runtime realm hash: " << FormatHash64(realmHash) << "\n";
  std::cout << "- runtime active nodes: " << activeNodes.GetCount() << "\n";
  std::cout << "- accepted packet send: " << (sentAcceptedPacket ? "ok" : "failed") << "\n";
  std::cout << "- accepted packet receive: " << (acceptedPacket.received ? "ok" : "failed") << "\n";
  std::cout << "- accepted packet peer: " << DescribeRuntimePeerAddress(acceptedPacket.peerAddress) << "\n";
  std::cout << "- accepted packet validation: " << DescribePacketValidationCode(acceptedPacket.validation.code) << "\n";
  std::cout << "- duplicate packet send: " << (sentDuplicatePacket ? "ok" : "failed") << "\n";
  std::cout << "- duplicate packet receive: " << (duplicatePacket.received ? "ok" : "failed") << "\n";
  std::cout << "- duplicate packet validation: " << DescribePacketValidationCode(duplicatePacket.validation.code) << "\n";
  std::cout << "- realm mismatch packet send: " << (sentMismatchedRealmPacket ? "ok" : "failed") << "\n";
  std::cout << "- realm mismatch packet receive: " << (mismatchedRealmPacket.received ? "ok" : "failed") << "\n";
  std::cout << "- realm mismatch packet validation: " << DescribePacketValidationCode(mismatchedRealmPacket.validation.code) << "\n";
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

  const bool runtimeValidationOk = relayStarted
      && acceptedPeerStarted
      && duplicatePeerStarted
      && mismatchedRealmPeerStarted
      && sentAcceptedPacket
      && acceptedPacket.received
      && acceptedPacket.validation.accepted
      && sentDuplicatePacket
      && duplicatePacket.received
      && duplicatePacket.validation.code == PacketValidationCode::DuplicateNodeId
      && sentMismatchedRealmPacket
      && mismatchedRealmPacket.received
      && mismatchedRealmPacket.validation.code == PacketValidationCode::RealmMismatch
      && activeNodes.GetCount() == 1;

  return runtimeValidationOk ? 0 : 1;
}