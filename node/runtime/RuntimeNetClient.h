#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct RuntimePeerAddress
{
  std::string host;
  int port = 0;
};

class RuntimeNetClient
{
 public:
  RuntimeNetClient() = default;
  ~RuntimeNetClient();

  RuntimeNetClient(const RuntimeNetClient&) = delete;
  RuntimeNetClient& operator=(const RuntimeNetClient&) = delete;

  bool Start(const RuntimePeerAddress& bindAddress);
  void Stop();

  bool IsRunning() const;
  bool SendPacket(const RuntimePeerAddress& peerAddress, const std::vector<uint8_t>& packet);
  std::string DescribeStack() const;

 private:
  uintptr_t socketHandle = UINTPTR_MAX;
  bool enetStarted = false;
  RuntimePeerAddress bindAddress = {};
};
