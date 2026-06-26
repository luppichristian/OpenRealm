#pragma once

#include "ColorMenu.h"
#include "ClientWorld.h"

class Game
{
 public:
  int Run();

 private:
  static constexpr int kScreenWidth = 1280;
  static constexpr int kScreenHeight = 720;

  void Initialize();
  void Shutdown();
  void ToggleColorMenu();

  World world;
  ClientWorld clientWorld;
  ColorMenu colorMenu;
};
