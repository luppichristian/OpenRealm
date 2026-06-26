#include "ColorMenu.h"
#include "Utils.h"

void ColorMenu::Toggle()
{
  isOpen = !isOpen;
}

bool ColorMenu::IsOpen() const
{
  return isOpen;
}

uint8_t ColorMenu::GetSelectedVoxelValue() const
{
  return selectedVoxelValue;
}

void ColorMenu::BuildCells(Cell cells[kCells], Rectangle* panelBounds) const
{
  const float panelPadding = 18.0f;
  const float cellSize = fmaxf(20.0f, fminf(34.0f, ((float)GetScreenWidth() - 160.0f) / (float)kColumns));
  const float gap = 6.0f;
  const float gridWidth = kColumns * cellSize + (kColumns - 1) * gap;
  const float gridHeight = kRows * cellSize + (kRows - 1) * gap;
  const float panelWidth = gridWidth + panelPadding * 2.0f;
  const float panelHeight = gridHeight + 92.0f;
  const float panelX = ((float)GetScreenWidth() - panelWidth) * 0.5f;
  const float panelY = ((float)GetScreenHeight() - panelHeight) * 0.5f;
  const float gridX = panelX + panelPadding;
  const float gridY = panelY + 58.0f;

  *panelBounds = {panelX, panelY, panelWidth, panelHeight};

  for (int cellIndex = 0; cellIndex < kCells; cellIndex++)
  {
    uint8_t voxelValue = (uint8_t)(cellIndex + 1);
    int column = cellIndex % kColumns;
    int row = cellIndex / kColumns;
    cells[cellIndex] = {
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

void ColorMenu::HandleClick()
{
  if (!isOpen || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

  Cell cells[kCells];
  Rectangle panelBounds = {};
  Vector2 mousePosition = GetMousePosition();
  BuildCells(cells, &panelBounds);

  for (int cellIndex = 0; cellIndex < kCells; cellIndex++)
  {
    if (!CheckCollisionPointRec(mousePosition, cells[cellIndex].bounds)) continue;
    selectedVoxelValue = cells[cellIndex].voxelValue;
    break;
  }
}

void ColorMenu::Draw() const
{
  if (!isOpen) return;

  Cell cells[kCells];
  Rectangle panelBounds = {};
  Vector2 mousePosition = GetMousePosition();
  BuildCells(cells, &panelBounds);

  DrawRectangleRounded(panelBounds, 0.08f, 8, Fade(BLACK, 0.88f));
  DrawRectangleLinesEx(panelBounds, 2.0f, Fade(RAYWHITE, 0.85f));
  DrawText("Voxel Color", (int)panelBounds.x + 18, (int)panelBounds.y + 16, 28, RAYWHITE);
  DrawText("Left click a swatch. 0 stays empty, 255 is pure white.", (int)panelBounds.x + 18, (int)panelBounds.y + 44, 18, Fade(RAYWHITE, 0.82f));

  for (int cellIndex = 0; cellIndex < kCells; cellIndex++)
  {
    Cell cell = cells[cellIndex];
    Color cellColor = GetVoxelDisplayColor(cell.voxelValue);
    bool isHovered = CheckCollisionPointRec(mousePosition, cell.bounds);
    bool isSelected = cell.voxelValue == selectedVoxelValue;

    DrawRectangleRec(cell.bounds, cellColor);
    if (isHovered) DrawRectangleRec(cell.bounds, Fade(WHITE, 0.18f));
    DrawRectangleLinesEx(cell.bounds, isSelected ? 3.0f : 1.0f, isSelected ? WHITE : Fade(BLACK, 0.45f));
  }
}
