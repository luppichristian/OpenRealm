#pragma once

#include "../Base.h"
#include "ClientConfigFiles.h"

#include <string>
#include <vector>

enum class ClientMenuMode
{
  None,
  Main,
  Play,
  Options,
  Credits,
  Pause,
};

enum class ClientMenuAction
{
  None,
  StartGame,
  ResumeGame,
  ReturnToMainMenu,
  ExitGame,
};

class ClientMenu
{
 public:
  void Initialize(const std::string& configPath);
  void OpenMainMenu();
  void OpenPauseMenu();
  void SetPlaying(bool playingNow);
  ClientMenuAction Update();
  void Draw(bool gameplayActive) const;

  bool IsOpen() const;
  const ClientFilesConfig& GetConfig() const;
  const ClientRealmState& GetSelectedRealmState() const;

 private:
  struct Button
  {
    Rectangle bounds = {};
    std::string label = {};
  };

  static constexpr float kMenuPanelWidth = 520.0f;
  static constexpr float kButtonHeight = 54.0f;

  static float ClampFloat(float value, float minimum, float maximum);
  static int ClampInt(int value, int minimum, int maximum);

  Button BuildCenteredButton(float top, const std::string& label) const;
  Button BuildBackButton() const;
  Button BuildFooterButton(float x, float y, float width, const std::string& label) const;
  Rectangle BuildPanelBounds() const;

  void LoadAvailableRealms();
  void RefreshSelectedRealm();
  void SaveConfig();
  void ApplyAudioSettings() const;

  bool HandleButton(const Button& button) const;
  bool HandleToggleRow(float top, const std::string& label, bool currentValue, bool* value);
  bool HandleStepperRow(float top, const std::string& label, const std::string& valueText, float* value, float step, float minimum, float maximum);
  bool HandleStepperRow(float top, const std::string& label, const std::string& valueText, int* value, int step, int minimum, int maximum);

  void DrawScreenTitle(const char* title, const char* subtitle) const;
  void DrawButton(const Button& button, bool primary) const;
  void DrawMainMenu() const;
  void DrawPlayMenu() const;
  void DrawOptionsMenu() const;
  void DrawCreditsMenu() const;
  void DrawPauseMenu() const;
  void DrawToggleRow(float top, const std::string& label, bool value) const;
  void DrawStepperRow(float top, const std::string& label, const std::string& valueText) const;
  void DrawRealmSummary(float top) const;
  void DrawMenuBackdrop(bool gameplayActive) const;

  ClientMenuMode mode = ClientMenuMode::Main;
  ClientMenuMode previousMode = ClientMenuMode::Main;
  ClientFilesConfig config = {};
  std::vector<std::string> availableRealms = {};
  ClientRealmState selectedRealm = {};
  std::string statusMessage = {};
  bool playing = false;
};
