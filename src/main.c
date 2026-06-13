#include "world.h"

#include <math.h>

#define COLOR_MENU_COLUMNS 17
#define COLOR_MENU_ROWS    15
#define COLOR_MENU_CELLS   (COLOR_MENU_COLUMNS * COLOR_MENU_ROWS)

typedef struct {
  Rectangle bounds;
  uint8_t voxelValue;
} ColorMenuCell;

static Color GetVoxelDisplayColor(uint8_t voxelValue) {
  if (voxelValue == 0) return BLANK;
  if (voxelValue == 255) return WHITE;

  float hue = ((float)(voxelValue - 1) / 254.0f) * 360.0f;
  return ColorFromHSV(hue, 0.85f, 1.0f);
}

static void BuildColorMenuCells(ColorMenuCell colorCells[COLOR_MENU_CELLS], Rectangle* panelBounds) {
  const float panelPadding = 18.0f;
  const float cellSize = fmaxf(20.0f, fminf(34.0f, ((float)GetScreenWidth() - 160.0f) / (float)COLOR_MENU_COLUMNS));
  const float gap = 6.0f;
  const float gridWidth = COLOR_MENU_COLUMNS * cellSize + (COLOR_MENU_COLUMNS - 1) * gap;
  const float gridHeight = COLOR_MENU_ROWS * cellSize + (COLOR_MENU_ROWS - 1) * gap;
  const float panelWidth = gridWidth + panelPadding * 2.0f;
  const float panelHeight = gridHeight + 92.0f;
  const float panelX = ((float)GetScreenWidth() - panelWidth) * 0.5f;
  const float panelY = ((float)GetScreenHeight() - panelHeight) * 0.5f;
  const float gridX = panelX + panelPadding;
  const float gridY = panelY + 58.0f;

  *panelBounds = (Rectangle) {panelX, panelY, panelWidth, panelHeight};

  for (int cellIndex = 0; cellIndex < COLOR_MENU_CELLS; cellIndex++) {
    uint8_t voxelValue = (uint8_t)(cellIndex + 1);
    int column = cellIndex % COLOR_MENU_COLUMNS;
    int row = cellIndex / COLOR_MENU_COLUMNS;
    colorCells[cellIndex] = (ColorMenuCell) {
        .bounds = {
                   gridX + (float)column * (cellSize + gap),
                   gridY + (float)row * (cellSize + gap),
                   cellSize,
                   cellSize,
                   },
        .voxelValue = voxelValue,
    };
  }
}

static void DrawHud(uint8_t selectedVoxelValue) {
  int screenCenterX = GetScreenWidth() / 2;
  int screenCenterY = GetScreenHeight() / 2;

  DrawLine(screenCenterX - 8, screenCenterY, screenCenterX + 8, screenCenterY, (Color) {120, 220, 255, 220});
  DrawLine(screenCenterX, screenCenterY - 8, screenCenterX, screenCenterY + 8, (Color) {120, 220, 255, 220});

  Color selectedColor = GetVoxelDisplayColor(selectedVoxelValue);
  DrawRectangle(16, 16, 52, 52, Fade(BLACK, 0.55f));
  DrawRectangle(24, 24, 36, 36, selectedColor);
  DrawRectangleLines(23, 23, 38, 38, RAYWHITE);
  DrawText("TAB color", 16, 74, 18, RAYWHITE);
}

static void DrawColorMenu(bool isColorMenuOpen, uint8_t selectedVoxelValue) {
  if (!isColorMenuOpen) return;

  ColorMenuCell colorCells[COLOR_MENU_CELLS];
  Rectangle panelBounds = {0};
  Vector2 mousePosition = GetMousePosition();
  BuildColorMenuCells(colorCells, &panelBounds);

  DrawRectangleRounded(panelBounds, 0.08f, 8, Fade(BLACK, 0.88f));
  DrawRectangleLinesEx(panelBounds, 2.0f, Fade(RAYWHITE, 0.85f));
  DrawText("Voxel Color", (int)panelBounds.x + 18, (int)panelBounds.y + 16, 28, RAYWHITE);
  DrawText("Left click a swatch. 0 stays empty, 255 is pure white.", (int)panelBounds.x + 18, (int)panelBounds.y + 44, 18, Fade(RAYWHITE, 0.82f));

  for (int cellIndex = 0; cellIndex < COLOR_MENU_CELLS; cellIndex++) {
    ColorMenuCell cell = colorCells[cellIndex];
    Color cellColor = GetVoxelDisplayColor(cell.voxelValue);
    bool isHovered = CheckCollisionPointRec(mousePosition, cell.bounds);
    bool isSelected = cell.voxelValue == selectedVoxelValue;

    DrawRectangleRec(cell.bounds, cellColor);
    if (isHovered) DrawRectangleRec(cell.bounds, Fade(WHITE, 0.18f));
    DrawRectangleLinesEx(cell.bounds, isSelected ? 3.0f : 1.0f, isSelected ? WHITE : Fade(BLACK, 0.45f));
  }
}

int main(void) {
  const int screenWidth = 1280;
  const int screenHeight = 720;

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(screenWidth, screenHeight, "openrealm");
  InitAudioDevice();
  InitWorld();
  DisableCursor();

  WorldEvent spawnLocalPlayerEvent = {
      .type = WORLD_EVENT_TYPE_SPAWN,
      .playerId = 0,
      .playerX = 0.0f,
      .playerY = 0.0f,
      .playerZ = 0.0f,
      .playerYaw = 0.0f,
      .playerPitch = -0.8f,
  };
  SendWorldEvent(&spawnLocalPlayerEvent);

  SetTargetFPS(60);

  bool isColorMenuOpen = false;
  uint8_t selectedVoxelValue = 255;

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_TAB)) {
      isColorMenuOpen = !isColorMenuOpen;
      if (isColorMenuOpen) {
        EnableCursor();
      } else {
        DisableCursor();
      }
    }

    if (!isColorMenuOpen) {
      Vector2 mouseDelta = GetMouseDelta();
      if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
        WorldEvent event = {
            .type = WORLD_EVENT_TYPE_LOOK,
            .playerId = 0,
            .lookDeltaX = mouseDelta.x,
            .lookDeltaY = mouseDelta.y,
        };
        SendWorldEvent(&event);
      }
    } else {
      GetMouseDelta();
    }

    WorldEvent moveEvent = {
        .type = WORLD_EVENT_TYPE_MOVE,
        .playerId = 0,
        .moveX = isColorMenuOpen ? 0.0f : (float)((IsKeyDown(KEY_D) ? 1 : 0) - (IsKeyDown(KEY_A) ? 1 : 0)),
        .moveY = isColorMenuOpen ? 0.0f : (float)((IsKeyDown(KEY_W) ? 1 : 0) - (IsKeyDown(KEY_S) ? 1 : 0)),
    };
    SendWorldEvent(&moveEvent);

    if (!isColorMenuOpen && IsKeyPressed(KEY_SPACE)) {
      WorldEvent event = {.type = WORLD_EVENT_TYPE_JUMP, .playerId = 0};
      SendWorldEvent(&event);
    }

    if (isColorMenuOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      ColorMenuCell colorCells[COLOR_MENU_CELLS];
      Rectangle panelBounds = {0};
      Vector2 mousePosition = GetMousePosition();
      BuildColorMenuCells(colorCells, &panelBounds);

      for (int cellIndex = 0; cellIndex < COLOR_MENU_CELLS; cellIndex++) {
        if (!CheckCollisionPointRec(mousePosition, colorCells[cellIndex].bounds)) continue;
        selectedVoxelValue = colorCells[cellIndex].voxelValue;
        break;
      }
    }

    if (!isColorMenuOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      WorldEvent event = {
          .type = WORLD_EVENT_TYPE_PLACE,
          .playerId = 0,
          .voxelValue = 0,
      };
      SendWorldEvent(&event);
    }

    if (!isColorMenuOpen && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
      WorldEvent event = {
          .type = WORLD_EVENT_TYPE_PLACE,
          .playerId = 0,
          .voxelValue = selectedVoxelValue,
      };
      SendWorldEvent(&event);
    }

    UpdateWorld();

    BeginDrawing();
    RenderWorld(0);
    DrawHud(selectedVoxelValue);
    DrawColorMenu(isColorMenuOpen, selectedVoxelValue);
    EndDrawing();
  }

  QuitWorld();
  CloseAudioDevice();
  CloseWindow();
  return 0;
}
