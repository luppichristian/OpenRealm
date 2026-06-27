#pragma once

#include "../TaskManager.h"
#include "ColorMenu.h"
#include "ClientWorld.h"

class Game
{
 public:
  int Run(TaskManager& taskManager);

 private:
  static constexpr int kScreenWidth = 1280;
  static constexpr int kScreenHeight = 720;

  void Initialize(TaskManager& taskManager);
  void Shutdown(TaskManager& taskManager);
  void ToggleColorMenu();

  World world;
  ClientWorld clientWorld;
  ColorMenu colorMenu;
};
