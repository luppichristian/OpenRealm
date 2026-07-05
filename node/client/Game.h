#pragma once

#include "../TaskManager.h"
#include "ClientMenu.h"
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
  void StartGameplay(TaskManager& taskManager);
  void StopGameplay();
  void ResumeGameplay();
  void OpenPauseMenu();
  void UpdateCursorState();
  void ToggleColorMenu();

  bool initialized = false;
  bool taskManagerStarted = false;
  bool gameplayActive = false;
  World world;
  ClientWorld clientWorld;
  ClientMenu clientMenu;
  ColorMenu colorMenu;
};
