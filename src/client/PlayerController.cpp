#include "PlayerController.h"

#include "../Utils.h"

void SendSpawnEvent(World& world)
{
  WorldEvent spawnLocalPlayerEvent = {
      .type = WorldEventType::Spawn,
      .playerId = LOCAL_PLAYER_ID,
      .playerX = 0.0f,
      .playerY = 0.0f,
      .playerZ = 0.0f,
      .playerYaw = 0.0f,
      .playerPitch = -0.8f,
  };
  world.SendEvent(spawnLocalPlayerEvent);
}

void HandleFrameInput(World& world, ColorMenu& colorMenu)
{
  if (!colorMenu.IsOpen())
  {
    Vector2 mouseDelta = GetMouseDelta();
    if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)
    {
      WorldEvent event = {
          .type = WorldEventType::Look,
          .playerId = LOCAL_PLAYER_ID,
          .lookDeltaX = mouseDelta.x,
          .lookDeltaY = mouseDelta.y,
      };
      world.SendEvent(event);
    }
  }
  else
  {
    GetMouseDelta();
  }

  WorldEvent moveEvent = {
      .type = WorldEventType::Move,
      .playerId = LOCAL_PLAYER_ID,
      .moveX = colorMenu.IsOpen() ? 0.0f : (float)((IsKeyDown(KEY_D) ? 1 : 0) - (IsKeyDown(KEY_A) ? 1 : 0)),
      .moveY = colorMenu.IsOpen() ? 0.0f : (float)((IsKeyDown(KEY_W) ? 1 : 0) - (IsKeyDown(KEY_S) ? 1 : 0)),
  };
  world.SendEvent(moveEvent);

  if (!colorMenu.IsOpen() && IsKeyPressed(KEY_SPACE))
  {
    WorldEvent event = {.type = WorldEventType::Jump, .playerId = LOCAL_PLAYER_ID};
    world.SendEvent(event);
  }

  colorMenu.HandleClick();

  if (colorMenu.IsOpen()) return;

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
  {
    WorldEvent event = {
        .type = WorldEventType::Place,
        .playerId = LOCAL_PLAYER_ID,
        .voxelValue = 0,
    };
    world.SendEvent(event);
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
  {
    WorldEvent event = {
        .type = WorldEventType::Place,
        .playerId = LOCAL_PLAYER_ID,
        .voxelValue = colorMenu.GetSelectedVoxelValue(),
    };
    world.SendEvent(event);
  }
}

void DrawHud(const ColorMenu& colorMenu)
{
  int screenCenterX = GetScreenWidth() / 2;
  int screenCenterY = GetScreenHeight() / 2;

  DrawLine(screenCenterX - 8, screenCenterY, screenCenterX + 8, screenCenterY, {120, 220, 255, 220});
  DrawLine(screenCenterX, screenCenterY - 8, screenCenterX, screenCenterY + 8, {120, 220, 255, 220});

  Color selectedColor = GetColorFromUint8(colorMenu.GetSelectedVoxelValue());
  DrawRectangle(16, 16, 52, 52, Fade(BLACK, 0.55f));
  DrawRectangle(24, 24, 36, 36, selectedColor);
  DrawRectangleLines(23, 23, 38, 38, RAYWHITE);
  DrawText("TAB color", 16, 74, 18, RAYWHITE);
}
