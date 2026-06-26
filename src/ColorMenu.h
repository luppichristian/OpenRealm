#pragma once

#include "Base.h"

class ColorMenu
{
 public:
  ColorMenu() = default;

  void Toggle();
  bool IsOpen() const;
  uint8_t GetSelectedVoxelValue() const;
  void HandleClick();
  void Draw() const;

 private:
  struct Cell
  {
    Rectangle bounds;
    uint8_t voxelValue;
  };

  static constexpr int kColumns = 17;
  static constexpr int kRows = 15;
  static constexpr int kCells = kColumns * kRows;

  void BuildCells(Cell cells[kCells], Rectangle* panelBounds) const;

  bool isOpen = false;
  uint8_t selectedVoxelValue = 255;
};
