#pragma once

#include "../TaskManager.h"
#include "../runtime/RuntimeSession.h"
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
  void SetCursorCaptured(bool captured);
  void UpdateCursorState();
  void ToggleColorMenu();
  void ApplyResolvedRuntimeSpawn();

  bool initialized = false;
  bool taskManagerStarted = false;
  bool gameplayActive = false;
  bool cursorCaptured = false;
  bool runtimeSpawnApplied = false;
  RuntimeWorldPosition appliedSpawnPosition = {};
  World world;
  ClientWorld clientWorld;
  RuntimeSession runtimeSession;
  ClientMenu clientMenu;
  ColorMenu colorMenu;
};
