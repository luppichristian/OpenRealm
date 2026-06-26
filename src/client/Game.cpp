#include "Game.h"

#include "PlayerController.h"

void Game::Initialize()
{
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(kScreenWidth, kScreenHeight, "openrealm");
  InitAudioDevice();
  world.Initialize();
  clientWorld.Initialize();
  DisableCursor();
  SendSpawnEvent(world);
}

void Game::Shutdown()
{
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

int Game::Run()
{
  Initialize();

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

  Shutdown();
  return 0;
}
