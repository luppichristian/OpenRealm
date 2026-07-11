#include "Args.h"
#include "RealmTester.h"

#include <iostream>

static uint32_t PacketCount(const std::array<uint32_t, 8>& counts, PacketType type)
{
  return counts[static_cast<size_t>(type)];
}

int main(int argc, char** argv)
{
  TesterOptions options = {};
  std::string error = {};
  if (!ParseTesterArguments(argc, argv, &options, &error))
  {
    if (!error.empty())
    {
      std::cout << "OpenRealm realm tester\n";
      std::cout << "- error: " << error << "\n";
    }
    return error.empty() ? 0 : 1;
  }

  RealmTester tester(options);
  TesterSummary summary = {};
  if (!tester.Run(&summary))
  {
    std::cout << "OpenRealm realm tester\n";
    std::cout << "- result: failed\n";
    std::cout << "- blockchain ready: " << (summary.blockchainReady ? 1 : 0) << "\n";
    std::cout << "- authenticated peers: " << summary.authenticatedPeers << "\n";
    std::cout << "- max topology nodes: " << summary.maxTopologyNodes << "\n";
    std::cout << "- observed handshake: " << (summary.sawHandshake ? 1 : 0) << "\n";
    std::cout << "- observed challenge request: " << (summary.sawChallengeRequest ? 1 : 0) << "\n";
    std::cout << "- observed challenge response: " << (summary.sawChallengeResponse ? 1 : 0) << "\n";
    std::cout << "- observed join response: " << (summary.sawJoinResponse ? 1 : 0) << "\n";
    std::cout << "- observed topology snapshot: " << (summary.sawTopologySnapshot ? 1 : 0) << "\n";
    std::cout << "- observed player snapshot: " << (summary.sawPlayerSnapshot ? 1 : 0) << "\n";
    std::cout << "- sent handshake/challenge_req/challenge_resp/join_req/player: "
              << PacketCount(summary.sentPacketCounts, PacketType::Handshake) << "/"
              << PacketCount(summary.sentPacketCounts, PacketType::ChallengeRequest) << "/"
              << PacketCount(summary.sentPacketCounts, PacketType::ChallengeResponse) << "/"
              << PacketCount(summary.sentPacketCounts, PacketType::JoinRequest) << "/"
              << PacketCount(summary.sentPacketCounts, PacketType::PlayerSnapshot) << "\n";
    std::cout << "- recv handshake/challenge_req/challenge_resp/join_resp/topology/player: "
              << PacketCount(summary.receivedPacketCounts, PacketType::Handshake) << "/"
              << PacketCount(summary.receivedPacketCounts, PacketType::ChallengeRequest) << "/"
              << PacketCount(summary.receivedPacketCounts, PacketType::ChallengeResponse) << "/"
              << PacketCount(summary.receivedPacketCounts, PacketType::JoinResponse) << "/"
              << PacketCount(summary.receivedPacketCounts, PacketType::TopologySnapshot) << "/"
              << PacketCount(summary.receivedPacketCounts, PacketType::PlayerSnapshot) << "\n";
    if (!summary.errorMessage.empty()) std::cout << "- error: " << summary.errorMessage << "\n";
    return 1;
  }

  std::cout << "OpenRealm realm tester\n";
  std::cout << "- result: passed\n";
  std::cout << "- blockchain ready: " << (summary.blockchainReady ? 1 : 0) << "\n";
  std::cout << "- authenticated peers: " << summary.authenticatedPeers << "\n";
  std::cout << "- max topology nodes: " << summary.maxTopologyNodes << "\n";
  std::cout << "- observed handshake: " << (summary.sawHandshake ? 1 : 0) << "\n";
  std::cout << "- observed challenge request: " << (summary.sawChallengeRequest ? 1 : 0) << "\n";
  std::cout << "- observed challenge response: " << (summary.sawChallengeResponse ? 1 : 0) << "\n";
  std::cout << "- observed join response: " << (summary.sawJoinResponse ? 1 : 0) << "\n";
  std::cout << "- observed topology snapshot: " << (summary.sawTopologySnapshot ? 1 : 0) << "\n";
  std::cout << "- observed player snapshot: " << (summary.sawPlayerSnapshot ? 1 : 0) << "\n";
  std::cout << "- sent handshake/challenge_req/challenge_resp/join_req/player: "
            << PacketCount(summary.sentPacketCounts, PacketType::Handshake) << "/"
            << PacketCount(summary.sentPacketCounts, PacketType::ChallengeRequest) << "/"
            << PacketCount(summary.sentPacketCounts, PacketType::ChallengeResponse) << "/"
            << PacketCount(summary.sentPacketCounts, PacketType::JoinRequest) << "/"
            << PacketCount(summary.sentPacketCounts, PacketType::PlayerSnapshot) << "\n";
  std::cout << "- recv handshake/challenge_req/challenge_resp/join_resp/topology/player: "
            << PacketCount(summary.receivedPacketCounts, PacketType::Handshake) << "/"
            << PacketCount(summary.receivedPacketCounts, PacketType::ChallengeRequest) << "/"
            << PacketCount(summary.receivedPacketCounts, PacketType::ChallengeResponse) << "/"
            << PacketCount(summary.receivedPacketCounts, PacketType::JoinResponse) << "/"
            << PacketCount(summary.receivedPacketCounts, PacketType::TopologySnapshot) << "/"
            << PacketCount(summary.receivedPacketCounts, PacketType::PlayerSnapshot) << "\n";
  return 0;
}
