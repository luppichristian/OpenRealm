#include "RuntimeNetClient.h"

#include <enet/enet.h>

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

RuntimeNetClient::~RuntimeNetClient()
{
  Stop();
}

bool RuntimeNetClient::Start(const RuntimePeerAddress& address)
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

void RuntimeNetClient::Stop()
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

bool RuntimeNetClient::IsRunning() const
{
  return socketHandle != UINTPTR_MAX;
}

bool RuntimeNetClient::SendPacket(const RuntimePeerAddress& peerAddress, const std::vector<uint8_t>& packet)
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

std::string RuntimeNetClient::DescribeStack() const
{
  return "ENet UDP binary packet transport";
}
