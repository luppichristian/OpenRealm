#include "RuntimeSession.h"

#include "ProtocolVersion.h"

#include "../client/PlayerController.h"

#include <algorithm>

namespace
{
constexpr uint64_t kJoinRetryIntervalMs = 500;
constexpr uint64_t kStaleNodeMultiplier = 6;
constexpr uint64_t kStalePlayerMultiplier = 10;
}

RuntimeSession::~RuntimeSession()
{
  Stop();
}

bool RuntimeSession::Start(const RuntimeSessionConfig& sessionConfig)
{
  Stop();
  config = sessionConfig;
  knownNodes = ActiveNodeBucket(config.maxKnownNodes);
  connectedNodeIds.clear();
  remotePlayers.clear();
  localNodePosition = config.initialNodePosition;
  resolvedSpawnPosition = config.joinTargetPosition;
  nowMs = 0;
  lastNeighborRefreshMs = 0;
  lastTopologyBroadcastMs = 0;
  lastPlayerBroadcastMs = 0;
  lastJoinRequestMs = 0;
  joinRequestToken = config.localNodeId ^ 0x9e3779b9u;
  joinResolved = !config.enabled || config.jumpNode.host.empty() || config.jumpNode.port <= 0;
  if (joinResolved)
  {
    resolvedSpawnPosition = localNodePosition;
  }

  if (!config.enabled)
  {
    running = true;
    return true;
  }

  if (!client.Start(config.bindAddress))
  {
    return false;
  }

  running = true;
  if (!config.jumpNode.host.empty() && config.jumpNode.port > 0)
  {
    SendPacketTo(config.jumpNode, MakeHandshakePacket(BuildLocalHandshake()));
    MaybeRequestJoin();
  }
  return true;
}

void RuntimeSession::Stop(World* world)
{
  if (world != nullptr)
  {
    for (const RemotePlayerReplica& replica : remotePlayers)
    {
      if (!replica.active) continue;
      WorldEvent event = {.type = WorldEventType::Despawn, .playerId = replica.playerId};
      world->SendEvent(event);
    }
  }

  remotePlayers.clear();
  connectedNodeIds.clear();
  knownNodes = ActiveNodeBucket(1);
  client.Stop();
  running = false;
  joinResolved = false;
  nowMs = 0;
}

bool RuntimeSession::IsRunning() const
{
  return running;
}

RuntimeWorldPosition RuntimeSession::GetResolvedSpawnPosition() const
{
  return resolvedSpawnPosition;
}

RuntimeSessionStatus RuntimeSession::GetStatus() const
{
  return {
      .running = running,
      .joinResolved = joinResolved,
      .localNodePosition = localNodePosition,
      .resolvedSpawnPosition = resolvedSpawnPosition,
      .knownNodes = knownNodes.GetCount(),
      .connectedNodes = connectedNodeIds.size(),
  };
}

void RuntimeSession::SetJoinTargetPosition(const RuntimeWorldPosition& position)
{
  config.joinTargetPosition = position;
  if (!joinResolved) resolvedSpawnPosition = position;
}

void RuntimeSession::SetNodePosition(const RuntimeWorldPosition& position)
{
  localNodePosition = position;
}

void RuntimeSession::Tick(uint32_t deltaMs, World* world)
{
  if (!running) return;
  nowMs += deltaMs;

  PumpIncomingPackets(world);
  if (!config.enabled) return;

  if (!joinResolved) MaybeRequestJoin();

  if (nowMs - lastNeighborRefreshMs >= config.neighborRefreshMs)
  {
    RefreshConnections();
    lastNeighborRefreshMs = nowMs;
  }

  if (nowMs - lastTopologyBroadcastMs >= config.topologyBroadcastMs)
  {
    BroadcastHandshake();
    BroadcastTopology();
    lastTopologyBroadcastMs = nowMs;
  }

  if (world != nullptr && nowMs - lastPlayerBroadcastMs >= config.playerBroadcastMs)
  {
    BroadcastLocalPlayer(world);
    lastPlayerBroadcastMs = nowMs;
  }

  PruneStaleState(world);
}

void RuntimeSession::PumpIncomingPackets(World* world)
{
  if (!config.enabled) return;

  while (true)
  {
    std::vector<uint8_t> bytes = {};
    RuntimePeerAddress peerAddress = {};
    if (!client.ReceivePacket(&bytes, &peerAddress, 0)) break;

    PacketValidationContext validationContext = {};
    validationContext.localNodeId = config.localNodeId;
    validationContext.expectedProtocolVersion = kRuntimeProtocolVersion;
    validationContext.expectedRealmHash = config.realmHash;
    validationContext.activeNodes = &knownNodes;
    validationContext.tick = nowMs;

    PacketValidationResult validation = ValidateIncomingPacket(bytes, peerAddress, validationContext);
    if (!validation.accepted)
    {
      TraceLog(LOG_WARNING, "runtime dropped packet from %s: %s", DescribeRuntimePeerAddress(peerAddress).c_str(), DescribePacketValidationCode(validation.code).c_str());
      continue;
    }

    HandleParsedPacket(validation.packet, peerAddress, world);
  }
}

void RuntimeSession::HandleParsedPacket(const Packet& packet, const RuntimePeerAddress& peerAddress, World* world)
{
  switch (packet.type)
  {
    case PacketType::Handshake:
      break;

    case PacketType::JoinRequest:
    {
      JoinRequestPacketData joinRequest = {};
      if (!TryDecodeJoinRequestPacket(packet, &joinRequest)) return;
      HandleJoinRequest(joinRequest, peerAddress);
    }
    break;

    case PacketType::JoinResponse:
    {
      JoinResponsePacketData joinResponse = {};
      if (!TryDecodeJoinResponsePacket(packet, &joinResponse, config.maxKnownNodes)) return;
      HandleJoinResponse(joinResponse);
    }
    break;

    case PacketType::TopologySnapshot:
    {
      TopologySnapshotPacketData topologySnapshot = {};
      if (!TryDecodeTopologySnapshotPacket(packet, &topologySnapshot, config.maxKnownNodes)) return;
      HandleTopologySnapshot(topologySnapshot);
    }
    break;

    case PacketType::PlayerSnapshot:
    {
      PlayerSnapshotPacketData playerSnapshot = {};
      if (!TryDecodePlayerSnapshotPacket(packet, &playerSnapshot)) return;
      HandlePlayerSnapshot(playerSnapshot, world);
    }
    break;

    default:
      TraceLog(LOG_WARNING, "runtime received unsupported packet type %s from %s", DescribePacketType(packet.type).c_str(), DescribeRuntimePeerAddress(peerAddress).c_str());
      break;
  }
}

void RuntimeSession::HandleJoinRequest(const JoinRequestPacketData& joinRequest, const RuntimePeerAddress& peerAddress)
{
  if (!config.acceptsJoins) return;

  std::vector<const ActiveNodeState*> closest = knownNodes.BuildClosestNodes(
      joinRequest.requestingNodeId,
      config.realmHash,
      joinRequest.targetPosition,
      std::min((size_t)joinRequest.maxCandidates, config.maxKnownNodes),
      true);

  JoinResponsePacketData response = {};
  response.respondingNodeId = config.localNodeId;
  response.requestToken = joinRequest.requestToken;
  response.resolvedPosition = localNodePosition;

  const float localDistance = ComputeRuntimeWorldDistanceSquared(localNodePosition, joinRequest.targetPosition);
  float resolvedDistance = localDistance;
  response.candidates.push_back(BuildLocalTopologyNode());

  for (const ActiveNodeState* node : closest)
  {
    if (node == nullptr) continue;
    response.candidates.push_back(ToTopologyNodeState(*node));
    const float candidateDistance = ComputeRuntimeWorldDistanceSquared(node->position, joinRequest.targetPosition);
    if (candidateDistance < resolvedDistance)
    {
      resolvedDistance = candidateDistance;
      response.resolvedPosition = node->position;
    }
  }

  SendPacketTo(peerAddress, MakeJoinResponsePacket(response));
}

void RuntimeSession::HandleJoinResponse(const JoinResponsePacketData& joinResponse)
{
  if (joinResponse.requestToken != joinRequestToken) return;

  for (const TopologyNodeState& node : joinResponse.candidates)
  {
    if (node.nodeId == config.localNodeId) continue;
    knownNodes.RegisterTopologyNode(node, 0, nowMs);
  }

  float bestDistance = ComputeRuntimeWorldDistanceSquared(joinResponse.resolvedPosition, config.joinTargetPosition);
  RuntimeWorldPosition bestPosition = joinResponse.resolvedPosition;
  for (const TopologyNodeState& node : joinResponse.candidates)
  {
    const float candidateDistance = ComputeRuntimeWorldDistanceSquared(node.position, config.joinTargetPosition);
    if (candidateDistance < bestDistance)
    {
      bestDistance = candidateDistance;
      bestPosition = node.position;
    }
  }

  resolvedSpawnPosition = bestPosition;
  localNodePosition = bestPosition;
  joinResolved = true;
}

void RuntimeSession::HandleTopologySnapshot(const TopologySnapshotPacketData& topologySnapshot)
{
  for (const TopologyNodeState& node : topologySnapshot.nodes)
  {
    if (node.nodeId == config.localNodeId) continue;
    if (node.realmHash != config.realmHash) continue;
    knownNodes.RegisterTopologyNode(node, 0, nowMs);
  }
}

void RuntimeSession::HandlePlayerSnapshot(const PlayerSnapshotPacketData& playerSnapshot, World* world)
{
  if (playerSnapshot.nodeId == config.localNodeId) return;
  ActiveNodeState* node = knownNodes.FindMutableByNodeId(playerSnapshot.nodeId);
  if (node != nullptr)
  {
    node->position = playerSnapshot.nodePosition;
    node->lastSeenTick = nowMs;
  }
  if (world == nullptr) return;
  ApplyRemotePlayerSnapshot(playerSnapshot, world);
}

void RuntimeSession::RefreshConnections()
{
  std::vector<const ActiveNodeState*> candidates = knownNodes.BuildNeighborCandidates(
      config.localNodeId,
      config.realmHash,
      localNodePosition,
      config.interestArea,
      config.maxNodeConnections);
  if (candidates.empty())
  {
    candidates = knownNodes.BuildClosestNodes(
        config.localNodeId,
        config.realmHash,
        localNodePosition,
        std::min((size_t)1, config.maxNodeConnections),
        false);
  }

  for (const ActiveNodeState& node : knownNodes.GetNodes())
  {
    knownNodes.MarkConnected(node.nodeId, false);
  }

  connectedNodeIds.clear();
  for (const ActiveNodeState* candidate : candidates)
  {
    if (candidate == nullptr) continue;
    connectedNodeIds.push_back(candidate->nodeId);
    knownNodes.MarkConnected(candidate->nodeId, true);
    SendPacketTo(candidate->peerAddress, MakeHandshakePacket(BuildLocalHandshake()));
  }
}

void RuntimeSession::BroadcastHandshake()
{
  SendPacketToConnectedPeers(MakeHandshakePacket(BuildLocalHandshake()));
  if (!config.jumpNode.host.empty() && config.jumpNode.port > 0)
  {
    SendPacketTo(config.jumpNode, MakeHandshakePacket(BuildLocalHandshake()));
  }
}

void RuntimeSession::BroadcastTopology()
{
  TopologySnapshotPacketData snapshot = {};
  snapshot.senderNodeId = config.localNodeId;
  snapshot.nodes.push_back(BuildLocalTopologyNode());

  std::vector<TopologyNodeState> knownSnapshot = knownNodes.BuildTopologySnapshot(
      config.localNodeId,
      config.realmHash,
      config.maxKnownNodes);
  snapshot.nodes.insert(snapshot.nodes.end(), knownSnapshot.begin(), knownSnapshot.end());
  SendPacketToConnectedPeers(MakeTopologySnapshotPacket(snapshot));
}

void RuntimeSession::BroadcastLocalPlayer(World* world)
{
  if (world == nullptr) return;
  const PlayerState* localPlayer = world->GetPlayerSystem().FindPlayer(LOCAL_PLAYER_ID);
  if (localPlayer == nullptr) return;

  localNodePosition = {
      .x = localPlayer->position.x,
      .y = localPlayer->position.y,
      .z = localPlayer->position.z,
  };

  PlayerSnapshotPacketData snapshot = {};
  snapshot.nodeId = config.localNodeId;
  snapshot.nodePosition = localNodePosition;
  snapshot.playerPosition = localNodePosition;
  snapshot.yaw = localPlayer->yaw;
  snapshot.pitch = localPlayer->pitch;
  snapshot.active = 1;
  SendPacketToConnectedPeers(MakePlayerSnapshotPacket(snapshot));
}

void RuntimeSession::MaybeRequestJoin()
{
  if (joinResolved) return;
  if (nowMs - lastJoinRequestMs < kJoinRetryIntervalMs) return;

  JoinRequestPacketData request = {};
  request.requestingNodeId = config.localNodeId;
  request.targetPosition = config.joinTargetPosition;
  request.maxCandidates = config.maxJoinCandidates;
  request.maxHops = config.maxJoinHops;
  request.requestToken = joinRequestToken;
  const Packet packet = MakeJoinRequestPacket(request);

  if (!config.jumpNode.host.empty() && config.jumpNode.port > 0)
  {
    SendPacketTo(config.jumpNode, packet);
  }
  for (uint32_t nodeId : connectedNodeIds)
  {
    const ActiveNodeState* node = knownNodes.FindByNodeId(nodeId);
    if (node == nullptr || !node->acceptsJoins) continue;
    SendPacketTo(node->peerAddress, packet);
  }
  lastJoinRequestMs = nowMs;
}

void RuntimeSession::PruneStaleState(World* world)
{
  const uint64_t staleNodeCutoff = nowMs > config.topologyBroadcastMs * kStaleNodeMultiplier
      ? nowMs - config.topologyBroadcastMs * kStaleNodeMultiplier
      : 0;
  knownNodes.ForgetStaleNodes(staleNodeCutoff);

  connectedNodeIds.erase(
      std::remove_if(connectedNodeIds.begin(), connectedNodeIds.end(), [&](uint32_t nodeId) {
        return knownNodes.FindByNodeId(nodeId) == nullptr;
      }),
      connectedNodeIds.end());

  const uint64_t stalePlayerCutoff = nowMs > config.playerBroadcastMs * kStalePlayerMultiplier
      ? nowMs - config.playerBroadcastMs * kStalePlayerMultiplier
      : 0;
  for (RemotePlayerReplica& replica : remotePlayers)
  {
    if (!replica.active || replica.lastSeenMs >= stalePlayerCutoff) continue;
    if (world != nullptr)
    {
      WorldEvent event = {.type = WorldEventType::Despawn, .playerId = replica.playerId};
      world->SendEvent(event);
    }
    replica = {};
  }
}

void RuntimeSession::ApplyRemotePlayerSnapshot(const PlayerSnapshotPacketData& playerSnapshot, World* world)
{
  if (world == nullptr) return;
  if (playerSnapshot.active == 0)
  {
    DespawnRemotePlayer(playerSnapshot.nodeId, world);
    return;
  }

  const int playerId = AcquireRemotePlayerId(playerSnapshot.nodeId);
  if (playerId < 0) return;

  WorldEvent event = {
      .type = WorldEventType::Spawn,
      .playerId = playerId,
      .playerX = playerSnapshot.playerPosition.x,
      .playerY = playerSnapshot.playerPosition.y,
      .playerZ = playerSnapshot.playerPosition.z,
      .playerYaw = playerSnapshot.yaw,
      .playerPitch = playerSnapshot.pitch,
  };
  world->SendEvent(event);

  for (RemotePlayerReplica& replica : remotePlayers)
  {
    if (!replica.active || replica.nodeId != playerSnapshot.nodeId) continue;
    replica.lastSeenMs = nowMs;
    return;
  }
}

int RuntimeSession::AcquireRemotePlayerId(uint32_t nodeId)
{
  for (RemotePlayerReplica& replica : remotePlayers)
  {
    if (replica.active && replica.nodeId == nodeId) return replica.playerId;
  }

  for (int playerId = 1; playerId < MAX_PLAYERS; ++playerId)
  {
    bool alreadyUsed = false;
    for (const RemotePlayerReplica& replica : remotePlayers)
    {
      if (replica.active && replica.playerId == playerId)
      {
        alreadyUsed = true;
        break;
      }
    }
    if (alreadyUsed) continue;

    remotePlayers.push_back({
        .nodeId = nodeId,
        .playerId = playerId,
        .lastSeenMs = nowMs,
        .active = true,
    });
    return playerId;
  }

  return -1;
}

void RuntimeSession::DespawnRemotePlayer(uint32_t nodeId, World* world)
{
  if (world == nullptr) return;
  for (RemotePlayerReplica& replica : remotePlayers)
  {
    if (!replica.active || replica.nodeId != nodeId) continue;
    WorldEvent event = {.type = WorldEventType::Despawn, .playerId = replica.playerId};
    world->SendEvent(event);
    replica = {};
    return;
  }
}

HandshakePacketData RuntimeSession::BuildLocalHandshake() const
{
  return {
      .protocolVersion = kRuntimeProtocolVersion,
      .nodeId = config.localNodeId,
      .realmHash = config.realmHash,
      .position = localNodePosition,
      .interestArea = config.interestArea,
      .nodeRole = (uint8_t)config.role,
      .acceptsJoins = (uint8_t)(config.acceptsJoins ? 1 : 0),
      .reserved = 0,
  };
}

TopologyNodeState RuntimeSession::BuildLocalTopologyNode() const
{
  return {
      .nodeId = config.localNodeId,
      .protocolVersion = kRuntimeProtocolVersion,
      .realmHash = config.realmHash,
      .peerAddress = client.GetBindAddress(),
      .position = localNodePosition,
      .interestArea = config.interestArea,
      .nodeRole = (uint8_t)config.role,
      .acceptsJoins = (uint8_t)(config.acceptsJoins ? 1 : 0),
  };
}

void RuntimeSession::SendPacketTo(const RuntimePeerAddress& peerAddress, const Packet& packet)
{
  if (!config.enabled) return;
  if (peerAddress.host.empty() || peerAddress.port <= 0) return;
  client.SendPacket(peerAddress, SerializePacket(packet));
}

void RuntimeSession::SendPacketToConnectedPeers(const Packet& packet)
{
  for (uint32_t nodeId : connectedNodeIds)
  {
    const ActiveNodeState* node = knownNodes.FindByNodeId(nodeId);
    if (node == nullptr) continue;
    SendPacketTo(node->peerAddress, packet);
  }
}
