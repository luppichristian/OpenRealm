#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct RuntimePeerAddress
{
  std::string host;
  int port = 0;
};

class RuntimeClient
{
 public:
  RuntimeClient() = default;
  ~RuntimeClient();

  RuntimeClient(const RuntimeClient&) = delete;
  RuntimeClient& operator=(const RuntimeClient&) = delete;

  bool Start(const RuntimePeerAddress& bindAddress);
  void Stop();

  bool IsRunning() const;
  const RuntimePeerAddress& GetBindAddress() const;
  bool SendPacket(const RuntimePeerAddress& peerAddress, const std::vector<uint8_t>& packet);
  bool ReceivePacket(std::vector<uint8_t>* packet, RuntimePeerAddress* peerAddress, uint32_t timeoutMs);
  std::string DescribeStack() const;

 private:
  uintptr_t socketHandle = UINTPTR_MAX;
  bool enetStarted = false;
  RuntimePeerAddress bindAddress = {};
};