#include "Game.h"

#include "PlayerController.h"

void Game::Initialize(TaskManager& taskManager)
{
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(kScreenWidth, kScreenHeight, "openrealm");
  InitAudioDevice();
  if (!taskManager.Start(MESH_WORKER_COUNT))
  {
    TraceLog(LOG_WARNING, "Failed to start global task manager; background tasks will be disabled.");
  }
  world.Initialize();
  clientWorld.Initialize(taskManager);
  DisableCursor();
  SendSpawnEvent(world);
}

void Game::Shutdown(TaskManager& taskManager)
{
  taskManager.Stop();
  clientWorld.Shutdown();
  world.Shutdown();
  CloseAudioDevice();
  CloseWindow();
}

void Game::ToggleColorMenu()
{
  colorMenu.Toggle();
  if (colorMenu.IsOpen())
  {
    EnableCursor();
  }
  else
  {
    DisableCursor();
  }
}

int Game::Run(TaskManager& taskManager)
{
  Initialize(taskManager);

  while (!WindowShouldClose())
  {
    if (IsKeyPressed(KEY_TAB)) ToggleColorMenu();

    HandleFrameInput(world, colorMenu);
    world.Update(GetFrameTime());
    clientWorld.Update(world);

    BeginDrawing();
    clientWorld.Render(world, LOCAL_PLAYER_ID);
    DrawFPS(GetScreenWidth() - 200, 16);
    DrawHud(colorMenu);
    colorMenu.Draw();
    EndDrawing();
  }

  Shutdown(taskManager);
  return 0;
}
