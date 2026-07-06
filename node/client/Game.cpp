#include "Game.h"

#include "PlayerController.h"

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
  SendSpawnEvent(world);
  gameplayActive = true;
  clientMenu.SetPlaying(true);
  ResumeGameplay();
}

void Game::StopGameplay()
{
  if (!gameplayActive) return;

  colorMenu = {};
  clientWorld.Shutdown();
  world.Shutdown();
  gameplayActive = false;
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
        world.Update(GetFrameTime());
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
