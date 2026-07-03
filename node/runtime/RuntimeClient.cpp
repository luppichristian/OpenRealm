#include "RuntimeClient.h"

#include <enet/enet.h>

static constexpr size_t MAX_RUNTIME_PACKET_BYTES = 64 * 1024;

static ENetSocket GetSocketHandle(uintptr_t socketHandle)
{
  return (ENetSocket)socketHandle;
}

static bool TryBuildSocketAddress(const RuntimePeerAddress& peerAddress, ENetAddress* socketAddress)
{
  if (socketAddress == nullptr) return false;
  if (peerAddress.port <= 0 || peerAddress.port > 65535) return false;

  ENetAddress address = {};
  address.port = (enet_uint16)peerAddress.port;

  if (peerAddress.host.empty())
  {
    address.host = ENET_HOST_ANY;
  }
  else if (enet_address_set_host(&address, peerAddress.host.c_str()) != 0)
  {
    return false;
  }

  *socketAddress = address;
  return true;
}

static bool TryBuildPeerAddress(const ENetAddress& socketAddress, RuntimePeerAddress* peerAddress)
{
  if (peerAddress == nullptr) return false;

  char hostName[256] = {};
  if (enet_address_get_host_ip(&socketAddress, hostName, sizeof(hostName)) != 0) return false;

  peerAddress->host = hostName;
  peerAddress->port = socketAddress.port;
  return true;
}

RuntimeClient::~RuntimeClient()
{
  Stop();
}

bool RuntimeClient::Start(const RuntimePeerAddress& address)
{
  Stop();

  ENetAddress socketAddress = {};
  if (!TryBuildSocketAddress(address, &socketAddress)) return false;

  if (enet_initialize() != 0) return false;
  enetStarted = true;

  ENetSocket udpSocket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
  if (udpSocket == ENET_SOCKET_NULL)
  {
    Stop();
    return false;
  }

  if (enet_socket_set_option(udpSocket, ENET_SOCKOPT_NONBLOCK, 1) != 0)
  {
    enet_socket_destroy(udpSocket);
    Stop();
    return false;
  }

  if (enet_socket_bind(udpSocket, &socketAddress) != 0)
  {
    enet_socket_destroy(udpSocket);
    Stop();
    return false;
  }

  socketHandle = (uintptr_t)udpSocket;
  bindAddress = address;
  return true;
}

void RuntimeClient::Stop()
{
  if (socketHandle != UINTPTR_MAX)
  {
    enet_socket_destroy(GetSocketHandle(socketHandle));
    socketHandle = UINTPTR_MAX;
  }

  if (enetStarted)
  {
    enet_deinitialize();
    enetStarted = false;
  }

  bindAddress = {};
}

bool RuntimeClient::IsRunning() const
{
  return socketHandle != UINTPTR_MAX;
}

const RuntimePeerAddress& RuntimeClient::GetBindAddress() const
{
  return bindAddress;
}

bool RuntimeClient::SendPacket(const RuntimePeerAddress& peerAddress, const std::vector<uint8_t>& packet)
{
  if (!IsRunning() || packet.empty()) return false;

  ENetAddress socketAddress = {};
  if (!TryBuildSocketAddress(peerAddress, &socketAddress)) return false;

  ENetBuffer buffer = {};
  buffer.data = (void*)packet.data();
  buffer.dataLength = packet.size();

  const int sentBytes = enet_socket_send(GetSocketHandle(socketHandle), &socketAddress, &buffer, 1);
  return sentBytes == (int)packet.size();
}

bool RuntimeClient::ReceivePacket(std::vector<uint8_t>* packet, RuntimePeerAddress* peerAddress, uint32_t timeoutMs)
{
  if (!IsRunning() || packet == nullptr || peerAddress == nullptr) return false;

  enet_uint32 socketWait = ENET_SOCKET_WAIT_RECEIVE;
  if (enet_socket_wait(GetSocketHandle(socketHandle), &socketWait, timeoutMs) != 0) return false;
  if ((socketWait & ENET_SOCKET_WAIT_RECEIVE) == 0) return false;

  std::vector<uint8_t> bufferBytes(MAX_RUNTIME_PACKET_BYTES);
  ENetBuffer buffer = {};
  buffer.data = bufferBytes.data();
  buffer.dataLength = bufferBytes.size();

  ENetAddress socketAddress = {};
  const int receivedBytes = enet_socket_receive(GetSocketHandle(socketHandle), &socketAddress, &buffer, 1);
  if (receivedBytes <= 0) return false;
  if (!TryBuildPeerAddress(socketAddress, peerAddress)) return false;

  packet->assign(bufferBytes.begin(), bufferBytes.begin() + receivedBytes);
  return true;
}

std::string RuntimeClient::DescribeStack() const
{
  return "ENet UDP binary packet transport";
}