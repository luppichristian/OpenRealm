#include "Game.h"

void Game::Initialize()
{
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(kScreenWidth, kScreenHeight, "openrealm");
  InitAudioDevice();
  world.Initialize();
  DisableCursor();
  playerController.SendSpawnEvent(world);
}

void Game::Shutdown()
{
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

int Game::Run()
{
  Initialize();

  while (!WindowShouldClose())
  {
    if (IsKeyPressed(KEY_TAB)) ToggleColorMenu();

    playerController.HandleFrameInput(world, colorMenu);
    world.Update();

    BeginDrawing();
    world.Render(PlayerController::kLocalPlayerId);
    DrawFPS(GetScreenWidth() - 200, 16);
    playerController.DrawHud(colorMenu);
    colorMenu.Draw();
    EndDrawing();
  }

  Shutdown();
  return 0;
}
