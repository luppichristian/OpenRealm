#include "Game.h"

#include "PlayerController.h"

#include "../blockchain/RealmConfigFiles.h"
#include "../runtime/NodeConfigFiles.h"
#include "../runtime/ProtocolVersion.h"
#include "../runtime/RuntimeRealm.h"

static RuntimeWorldPosition BuildFallbackSpawnPosition(const ClientFilesConfig& clientConfig)
{
  return clientConfig.joinTargetPosition;
}

void Game::Initialize(TaskManager& taskManager)
{
  if (initialized) return;

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(kScreenWidth, kScreenHeight, "openrealm");
  SetExitKey(KEY_NULL);
  InitAudioDevice();
  taskManagerStarted = taskManager.Start(MESH_WORKER_COUNT);
  if (!taskManagerStarted)
  {
    TraceLog(LOG_WARNING, "Failed to start global task manager; background tasks will be disabled.");
  }

  clientMenu.Initialize("config.json");
  clientMenu.OpenMainMenu();
  cursorCaptured = true;
  UpdateCursorState();
  initialized = true;
}

void Game::Shutdown(TaskManager& taskManager)
{
  if (!initialized) return;

  StopGameplay();
  if (taskManagerStarted)
  {
    taskManager.Stop();
    taskManagerStarted = false;
  }
  SetCursorCaptured(false);
  CloseAudioDevice();
  CloseWindow();
  initialized = false;
}

void Game::StartGameplay(TaskManager& taskManager)
{
  StopGameplay();
  world.Initialize();
  clientWorld.Initialize(taskManager);
  colorMenu = {};
  runtimeSpawnApplied = false;
  appliedSpawnPosition = {};

  const ClientFilesConfig& clientConfig = clientMenu.GetConfig();
  const ClientRealmState& selectedRealm = clientMenu.GetSelectedRealmState();
  RuntimeWorldPosition spawnPosition = BuildFallbackSpawnPosition(clientConfig);

  std::string nodeConfigError = {};
  NodeFilesConfig nodeConfig = {};
  if (!LoadNodeFilesConfig(clientConfig.configPath, &nodeConfig, &nodeConfigError))
  {
    TraceLog(LOG_WARNING, "Failed to load node config for runtime session: %s", nodeConfigError.c_str());
  }
  else
  {
    std::string realmError = {};
    RealmConfigFiles realmFiles = {};
    if (!LoadRealmConfigFiles(selectedRealm.directory, &realmFiles, &realmError))
    {
      TraceLog(LOG_WARNING, "Failed to load realm files for runtime session: %s", realmError.c_str());
    }
    else
    {
      RuntimePeerAddress jumpNode = {};
      if (!selectedRealm.jumpNodes.empty() && clientConfig.jumpNodeIndex >= 0 && (size_t)clientConfig.jumpNodeIndex < selectedRealm.jumpNodes.size())
      {
        jumpNode.host = selectedRealm.jumpNodes[(size_t)clientConfig.jumpNodeIndex].host;
        jumpNode.port = selectedRealm.jumpNodes[(size_t)clientConfig.jumpNodeIndex].port;
      }

      RuntimeRealmState runtimeRealmState = {};
      runtimeRealmState.runtimeProtocolVersion = kRuntimeProtocolVersion;
      runtimeRealmState.chainId = realmFiles.directory;
      runtimeRealmState.blockchainConfig = realmFiles.blockchainConfig;
      const uint64_t realmHash = ComputeRuntimeRealmHash(runtimeRealmState);

      RuntimeSessionConfig sessionConfig = {};
      sessionConfig.role = RuntimeNodeRole::Client;
      sessionConfig.bindAddress = nodeConfig.runtimeBindAddress;
      sessionConfig.jumpNode = jumpNode;
      sessionConfig.localNodeId = nodeConfig.runtimeNodeId;
      sessionConfig.realmHash = realmHash;
      sessionConfig.initialNodePosition = clientConfig.joinTargetPosition;
      sessionConfig.interestArea = nodeConfig.runtimeInterestArea;
      sessionConfig.joinTargetPosition = clientConfig.joinTargetPosition;
      sessionConfig.receiveTimeoutMs = nodeConfig.runtimeReceiveTimeoutMs;
      sessionConfig.maxNodeConnections = nodeConfig.runtimeMaxNodeConnections;
      sessionConfig.maxKnownNodes = nodeConfig.runtimeMaxKnownNodes;
      sessionConfig.maxJoinCandidates = nodeConfig.runtimeJoinCandidateCount;
      sessionConfig.maxJoinHops = nodeConfig.runtimeJoinMaxHops;
      sessionConfig.neighborRefreshMs = nodeConfig.runtimeNeighborRefreshMs;
      sessionConfig.topologyBroadcastMs = nodeConfig.runtimeTopologyBroadcastMs;
      sessionConfig.playerBroadcastMs = nodeConfig.runtimePlayerBroadcastMs;
      sessionConfig.enabled = nodeConfig.runtimeEnabled;
      sessionConfig.acceptsJoins = nodeConfig.runtimeAcceptsJoins;

      if (!runtimeSession.Start(sessionConfig))
      {
        TraceLog(LOG_WARNING, "Failed to start runtime session; continuing in local-only mode.");
      }
      else
      {
        spawnPosition = runtimeSession.GetResolvedSpawnPosition();
      }
    }
  }

  SendSpawnEvent(world, spawnPosition);
  appliedSpawnPosition = spawnPosition;
  runtimeSpawnApplied = true;
  gameplayActive = true;
  clientMenu.SetPlaying(true);
  ResumeGameplay();
}

void Game::StopGameplay()
{
  if (!gameplayActive) return;

  colorMenu = {};
  runtimeSession.Stop(&world);
  clientWorld.Shutdown();
  world.Shutdown();
  gameplayActive = false;
  runtimeSpawnApplied = false;
  appliedSpawnPosition = {};
  clientMenu.SetPlaying(false);
  UpdateCursorState();
}

void Game::ResumeGameplay()
{
  if (!gameplayActive) return;
  clientMenu.SetPlaying(true);
  UpdateCursorState();
}

void Game::OpenPauseMenu()
{
  if (!gameplayActive) return;
  colorMenu = {};
  clientMenu.OpenPauseMenu();
  UpdateCursorState();
}

void Game::SetCursorCaptured(bool captured)
{
  if (captured == cursorCaptured) return;

  cursorCaptured = captured;
  if (cursorCaptured)
  {
    HideCursor();
    DisableCursor();
  }
  else
  {
    EnableCursor();
    ShowCursor();
  }
}

void Game::UpdateCursorState()
{
  const bool shouldCaptureCursor = gameplayActive && !clientMenu.IsOpen() && !colorMenu.IsOpen() && IsWindowFocused();
  SetCursorCaptured(shouldCaptureCursor);
}

void Game::ToggleColorMenu()
{
  if (!gameplayActive || clientMenu.IsOpen()) return;
  colorMenu.Toggle();
  UpdateCursorState();
}

void Game::ApplyResolvedRuntimeSpawn()
{
  if (!gameplayActive || !runtimeSession.IsRunning()) return;

  const RuntimeSessionStatus status = runtimeSession.GetStatus();
  if (!status.joinResolved) return;
  if (runtimeSpawnApplied && ComputeRuntimeWorldDistanceSquared(appliedSpawnPosition, status.resolvedSpawnPosition) < 0.0001f)
  {
    return;
  }

  SendSpawnEvent(world, status.resolvedSpawnPosition);
  appliedSpawnPosition = status.resolvedSpawnPosition;
  runtimeSpawnApplied = true;
}

int Game::Run(TaskManager& taskManager)
{
  Initialize(taskManager);

  while (!WindowShouldClose())
  {
    const ClientMenuAction menuAction = clientMenu.Update();
    switch (menuAction)
    {
      case ClientMenuAction::StartGame:
        StartGameplay(taskManager);
        break;
      case ClientMenuAction::ResumeGame:
        ResumeGameplay();
        break;
      case ClientMenuAction::ReturnToMainMenu:
        StopGameplay();
        clientMenu.OpenMainMenu();
        break;
      case ClientMenuAction::ExitGame:
        Shutdown(taskManager);
        return 0;
      case ClientMenuAction::None: break;
    }

    if (gameplayActive && !clientMenu.IsOpen())
    {
      if (IsKeyPressed(KEY_ESCAPE))
      {
        OpenPauseMenu();
      }
      else
      {
        if (IsKeyPressed(KEY_TAB)) ToggleColorMenu();

        PlayerInputOptions inputOptions = {
            .mouseSensitivity = clientMenu.GetConfig().mouseSensitivity,
            .invertMouseY = clientMenu.GetConfig().invertMouseY,
        };
        HandleFrameInput(world, colorMenu, inputOptions);
        ApplyResolvedRuntimeSpawn();
        world.Update(GetFrameTime());
        runtimeSession.Tick((uint32_t)(GetFrameTime() * 1000.0f + 0.5f), &world);
        clientWorld.Update(world);
      }
    }

    BeginDrawing();
    if (gameplayActive)
    {
      clientWorld.Render(world, LOCAL_PLAYER_ID);
      if (clientMenu.GetConfig().showFps) DrawFPS(GetScreenWidth() - 200, 16);
      DrawHud(colorMenu);
      colorMenu.Draw();
    }
    else
    {
      ClearBackground(BLACK);
    }

    clientMenu.Draw(gameplayActive);
    EndDrawing();
    UpdateCursorState();
  }

  Shutdown(taskManager);
  return 0;
}
