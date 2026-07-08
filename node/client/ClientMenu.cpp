#include "ClientMenu.h"

#include <algorithm>
#include <cstdio>

static constexpr float kPanelPadding = 28.0f;
static constexpr float kPlayRowTop = 184.0f;
static constexpr float kOptionsRowTop = 140.0f;
static constexpr float kRowStep = 68.0f;

struct StepperRowLayout
{
  Rectangle labelBounds = {};
  Rectangle leftButton = {};
  Rectangle valueBounds = {};
  Rectangle rightButton = {};
};

static StepperRowLayout BuildStepperRowLayout(const Rectangle& panel, float top)
{
  const float controlsWidth = fminf(276.0f, panel.width * 0.58f);
  const float leftButtonWidth = 42.0f;
  const float rightButtonWidth = 42.0f;
  const float gap = 6.0f;
  const float valueWidth = controlsWidth - leftButtonWidth - rightButtonWidth - gap * 2.0f;
  const float controlsLeft = panel.x + panel.width - kPanelPadding - controlsWidth;
  return {
      .labelBounds = {panel.x + kPanelPadding, top, controlsLeft - panel.x - kPanelPadding - 16.0f, 42.0f},
      .leftButton = {controlsLeft, top, leftButtonWidth, 42.0f},
      .valueBounds = {controlsLeft + leftButtonWidth + gap, top, valueWidth, 42.0f},
      .rightButton = {controlsLeft + leftButtonWidth + gap + valueWidth + gap, top, rightButtonWidth, 42.0f},
  };
}

static std::string FitTextToWidth(const std::string& text, int fontSize, float maxWidth)
{
  if (text.empty()) return text;
  if (MeasureText(text.c_str(), fontSize) <= maxWidth) return text;

  std::string clipped = text;
  while (!clipped.empty())
  {
    clipped.pop_back();
    const std::string candidate = clipped + "...";
    if (MeasureText(candidate.c_str(), fontSize) <= maxWidth) return candidate;
  }

  return "...";
}

static float DrawWrappedText(const std::string& text, float x, float y, float maxWidth, int fontSize, float lineSpacing, Color color)
{
  std::string line = {};
  std::string word = {};
  float currentY = y;

  auto flushLine = [&](const std::string& value) {
    if (value.empty()) return;
    DrawText(value.c_str(), (int)x, (int)currentY, fontSize, color);
    currentY += (float)fontSize + lineSpacing;
  };

  for (size_t i = 0; i <= text.size(); ++i)
  {
    const bool end = i == text.size();
    const char ch = end ? '\0' : text[i];
    if (ch == '\n')
    {
      if (!word.empty())
      {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureText(candidate.c_str(), fontSize) > maxWidth)
        {
          flushLine(line);
          line = word;
        }
        else
        {
          line = candidate;
        }
        word.clear();
      }
      flushLine(line);
      line.clear();
      continue;
    }

    if (end || ch == ' ')
    {
      if (!word.empty())
      {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureText(candidate.c_str(), fontSize) > maxWidth)
        {
          flushLine(line);
          line = word;
        }
        else
        {
          line = candidate;
        }
        word.clear();
      }
      continue;
    }

    word.push_back(ch);
  }

  flushLine(line);
  return currentY;
}

static Color GetMenuTextColor(bool hovered)
{
  return hovered ? WHITE : Fade(RAYWHITE, 0.92f);
}

float ClientMenu::ClampFloat(float value, float minimum, float maximum)
{
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

int ClientMenu::ClampInt(int value, int minimum, int maximum)
{
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

Rectangle ClientMenu::BuildPanelBounds() const
{
  const float width = fminf(720.0f, (float)GetScreenWidth() - 80.0f);
  const float height = fminf(640.0f, (float)GetScreenHeight() - 80.0f);
  return {
      ((float)GetScreenWidth() - width) * 0.5f,
      ((float)GetScreenHeight() - height) * 0.5f,
      width,
      height,
  };
}

ClientMenu::Button ClientMenu::BuildCenteredButton(float top, const std::string& label) const
{
  Rectangle panel = BuildPanelBounds();
  const float width = panel.width - 96.0f;
  return {
      .bounds = {
                 panel.x + (panel.width - width) * 0.5f,
                 top,
                 width,
                 kButtonHeight,
                 },
      .label = label,
  };
}

ClientMenu::Button ClientMenu::BuildBackButton() const
{
  Rectangle panel = BuildPanelBounds();
  return BuildFooterButton(panel.x + kPanelPadding, panel.y + panel.height - 72.0f, 140.0f, "Back");
}

ClientMenu::Button ClientMenu::BuildFooterButton(float x, float y, float width, const std::string& label) const
{
  return {
      .bounds = {x, y, width, 46.0f},
      .label = label,
  };
}

void ClientMenu::Initialize(const std::string& configPath)
{
  config.configPath = configPath;
  statusMessage.clear();

  std::string error = {};
  if (!LoadClientFilesConfig(configPath, &config, &error))
  {
    statusMessage = "Using default client config: " + error;
  }

  config.masterVolume = ClampFloat(config.masterVolume, 0.0f, 1.0f);
  config.mouseSensitivity = ClampFloat(config.mouseSensitivity, 0.25f, 2.5f);
  if (config.jumpNodeIndex < 0) config.jumpNodeIndex = 0;

  LoadAvailableRealms();
  RefreshSelectedRealm();
  ApplyAudioSettings();
}

void ClientMenu::OpenMainMenu()
{
  previousMode = mode;
  mode = ClientMenuMode::Main;
}

void ClientMenu::OpenPauseMenu()
{
  previousMode = mode;
  mode = ClientMenuMode::Pause;
}

void ClientMenu::SetPlaying(bool playingNow)
{
  playing = playingNow;
  if (!playing && mode == ClientMenuMode::None) mode = ClientMenuMode::Main;
}

bool ClientMenu::IsOpen() const
{
  return mode != ClientMenuMode::None;
}

const ClientFilesConfig& ClientMenu::GetConfig() const
{
  return config;
}

const ClientRealmState& ClientMenu::GetSelectedRealmState() const
{
  return selectedRealm;
}

void ClientMenu::LoadAvailableRealms()
{
  availableRealms = DiscoverClientRealmDirectories();
  if (availableRealms.empty())
  {
    availableRealms.push_back(config.selectedRealm);
  }

  if (std::find(availableRealms.begin(), availableRealms.end(), config.selectedRealm) == availableRealms.end())
  {
    availableRealms.push_back(config.selectedRealm);
    std::sort(availableRealms.begin(), availableRealms.end());
  }
}

void ClientMenu::RefreshSelectedRealm()
{
  if (availableRealms.empty())
  {
    selectedRealm = {};
    selectedRealm.directory = config.selectedRealm;
    selectedRealm.realmName = "unavailable";
    config.jumpNodeIndex = 0;
    return;
  }

  if (std::find(availableRealms.begin(), availableRealms.end(), config.selectedRealm) == availableRealms.end())
  {
    config.selectedRealm = availableRealms.front();
  }

  std::string error = {};
  if (!LoadClientRealmState(config.selectedRealm, &selectedRealm, &error))
  {
    selectedRealm = {};
    selectedRealm.directory = config.selectedRealm;
    selectedRealm.realmName = "unavailable";
    statusMessage = error;
  }

  if (selectedRealm.jumpNodes.empty())
  {
    config.jumpNodeIndex = 0;
  }
  else
  {
    config.jumpNodeIndex = ClampInt(config.jumpNodeIndex, 0, (int)selectedRealm.jumpNodes.size() - 1);
  }
}

void ClientMenu::SaveConfig()
{
  std::string error = {};
  if (!SaveClientFilesConfig(config, &error))
  {
    statusMessage = "Failed to save config: " + error;
    return;
  }

  statusMessage = "Saved client settings to " + config.configPath;
}

void ClientMenu::ApplyAudioSettings() const
{
  SetMasterVolume(config.masterVolume);
}

bool ClientMenu::HandleButton(const Button& button) const
{
  return CheckCollisionPointRec(GetMousePosition(), button.bounds) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

bool ClientMenu::HandleToggleRow(float top, const std::string& label, bool currentValue, bool* value)
{
  if (value == nullptr) return false;

  Rectangle panel = BuildPanelBounds();
  Rectangle buttonBounds = {
      panel.x + panel.width - 168.0f,
      top,
      120.0f,
      42.0f,
  };

  if (!CheckCollisionPointRec(GetMousePosition(), buttonBounds) || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return false;
  *value = !currentValue;
  SaveConfig();
  return true;
}

bool ClientMenu::HandleStepperRow(
    float top,
    const std::string& label,
    const std::string& valueText,
    float* value,
    float step,
    float minimum,
    float maximum)
{
  if (value == nullptr)
  {
    return false;
  }

  Rectangle panel = BuildPanelBounds();
  const StepperRowLayout layout = BuildStepperRowLayout(panel, top);

  bool changed = false;
  if (CheckCollisionPointRec(GetMousePosition(), layout.leftButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
  {
    *value = ClampFloat(*value - step, minimum, maximum);
    changed = true;
  }
  else if (CheckCollisionPointRec(GetMousePosition(), layout.rightButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
  {
    *value = ClampFloat(*value + step, minimum, maximum);
    changed = true;
  }

  if (!changed) return false;
  ApplyAudioSettings();
  SaveConfig();
  return true;
}

bool ClientMenu::HandleStepperRow(
    float top,
    const std::string& label,
    const std::string& valueText,
    int* value,
    int step,
    int minimum,
    int maximum)
{
  if (value == nullptr)
  {
    return false;
  }

  Rectangle panel = BuildPanelBounds();
  const StepperRowLayout layout = BuildStepperRowLayout(panel, top);

  bool changed = false;
  if (CheckCollisionPointRec(GetMousePosition(), layout.leftButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
  {
    *value = ClampInt(*value - step, minimum, maximum);
    changed = true;
  }
  else if (CheckCollisionPointRec(GetMousePosition(), layout.rightButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
  {
    *value = ClampInt(*value + step, minimum, maximum);
    changed = true;
  }

  if (!changed) return false;
  SaveConfig();
  return true;
}

ClientMenuAction ClientMenu::Update()
{
  if (mode == ClientMenuMode::None) return ClientMenuAction::None;

  if (IsKeyPressed(KEY_ESCAPE))
  {
    if (mode == ClientMenuMode::Pause)
    {
      mode = ClientMenuMode::None;
      return ClientMenuAction::ResumeGame;
    }
    if (mode == ClientMenuMode::Main) return ClientMenuAction::None;
    if (mode == ClientMenuMode::Options || mode == ClientMenuMode::Credits || mode == ClientMenuMode::Play)
    {
      mode = playing ? ClientMenuMode::Pause : ClientMenuMode::Main;
      return ClientMenuAction::None;
    }
  }

  Rectangle panel = BuildPanelBounds();
  const float firstButtonTop = panel.y + 140.0f;
  const float buttonStep = 68.0f;

  switch (mode)
  {
    case ClientMenuMode::Main:
      if (HandleButton(BuildCenteredButton(firstButtonTop, "Play")))
      {
        mode = ClientMenuMode::Play;
      }
      else if (HandleButton(BuildCenteredButton(firstButtonTop + buttonStep, "Options")))
      {
        previousMode = mode;
        mode = ClientMenuMode::Options;
      }
      else if (HandleButton(BuildCenteredButton(firstButtonTop + buttonStep * 2.0f, "Credits")))
      {
        previousMode = mode;
        mode = ClientMenuMode::Credits;
      }
      else if (HandleButton(BuildCenteredButton(firstButtonTop + buttonStep * 3.0f, "Quit")))
      {
        return ClientMenuAction::ExitGame;
      }
      break;

    case ClientMenuMode::Play:
    {
      int realmIndex = 0;
      for (size_t i = 0; i < availableRealms.size(); ++i)
      {
        if (availableRealms[i] == config.selectedRealm)
        {
          realmIndex = (int)i;
          break;
        }
      }

      const int realmMaximum = (int)availableRealms.size() - 1;
      if (HandleStepperRow(panel.y + kPlayRowTop, "Realm", selectedRealm.realmName, &realmIndex, 1, 0, realmMaximum))
      {
        config.selectedRealm = availableRealms[(size_t)realmIndex];
        RefreshSelectedRealm();
        SaveConfig();
      }

      const std::string jumpNodeLabel = selectedRealm.jumpNodes.empty()
                                            ? std::string("none")
                                            : selectedRealm.jumpNodes[(size_t)config.jumpNodeIndex].label;
      const int jumpNodeMaximum = selectedRealm.jumpNodes.empty() ? 0 : (int)selectedRealm.jumpNodes.size() - 1;
      HandleStepperRow(panel.y + kPlayRowTop + kRowStep, "Jump node", jumpNodeLabel, &config.jumpNodeIndex, 1, 0, jumpNodeMaximum);

      char targetXText[32] = {};
      std::snprintf(targetXText, sizeof(targetXText), "%.0f", config.joinTargetPosition.x);
      HandleStepperRow(panel.y + kPlayRowTop + kRowStep * 2.0f, "Join target X", targetXText, &config.joinTargetPosition.x, 16.0f, -4096.0f, 4096.0f);

      char targetYText[32] = {};
      std::snprintf(targetYText, sizeof(targetYText), "%.0f", config.joinTargetPosition.y);
      HandleStepperRow(panel.y + kPlayRowTop + kRowStep * 3.0f, "Join target Y", targetYText, &config.joinTargetPosition.y, 16.0f, -4096.0f, 4096.0f);

      char targetZText[32] = {};
      std::snprintf(targetZText, sizeof(targetZText), "%.0f", config.joinTargetPosition.z);
      HandleStepperRow(panel.y + kPlayRowTop + kRowStep * 4.0f, "Join target Z", targetZText, &config.joinTargetPosition.z, 8.0f, -512.0f, 2048.0f);

      if (HandleButton(BuildFooterButton(panel.x + panel.width - 284.0f, panel.y + panel.height - 72.0f, 124.0f, "Play")))
      {
        SaveConfig();
        mode = ClientMenuMode::None;
        return ClientMenuAction::StartGame;
      }
      if (HandleButton(BuildFooterButton(panel.x + panel.width - 144.0f, panel.y + panel.height - 72.0f, 116.0f, "Back")))
      {
        mode = ClientMenuMode::Main;
      }
    }
    break;

    case ClientMenuMode::Options:
    {
      char volumeText[32] = {};
      std::snprintf(volumeText, sizeof(volumeText), "%d%%", (int)(config.masterVolume * 100.0f + 0.5f));
      if (HandleStepperRow(panel.y + kOptionsRowTop, "Master volume", volumeText, &config.masterVolume, 0.1f, 0.0f, 1.0f))
      {
        ApplyAudioSettings();
      }

      char sensitivityText[32] = {};
      std::snprintf(sensitivityText, sizeof(sensitivityText), "%.2fx", config.mouseSensitivity);
      HandleStepperRow(panel.y + kOptionsRowTop + kRowStep, "Mouse sensitivity", sensitivityText, &config.mouseSensitivity, 0.1f, 0.25f, 2.5f);
      HandleToggleRow(panel.y + kOptionsRowTop + kRowStep * 2.0f, "Invert mouse Y", config.invertMouseY, &config.invertMouseY);
      HandleToggleRow(panel.y + kOptionsRowTop + kRowStep * 3.0f, "Show FPS", config.showFps, &config.showFps);

      if (HandleButton(BuildBackButton()))
      {
        mode = playing ? ClientMenuMode::Pause : ClientMenuMode::Main;
      }
    }
    break;

    case ClientMenuMode::Credits:
      if (HandleButton(BuildBackButton()))
      {
        mode = playing ? ClientMenuMode::Pause : ClientMenuMode::Main;
      }
      break;

    case ClientMenuMode::Pause:
      if (HandleButton(BuildCenteredButton(firstButtonTop, "Resume")))
      {
        mode = ClientMenuMode::None;
        return ClientMenuAction::ResumeGame;
      }
      if (HandleButton(BuildCenteredButton(firstButtonTop + buttonStep, "Options")))
      {
        previousMode = mode;
        mode = ClientMenuMode::Options;
      }
      else if (HandleButton(BuildCenteredButton(firstButtonTop + buttonStep * 2.0f, "Return to main menu")))
      {
        mode = ClientMenuMode::Main;
        return ClientMenuAction::ReturnToMainMenu;
      }
      else if (HandleButton(BuildCenteredButton(firstButtonTop + buttonStep * 3.0f, "Quit")))
      {
        return ClientMenuAction::ExitGame;
      }
      break;

    case ClientMenuMode::None: break;
  }

  return ClientMenuAction::None;
}

void ClientMenu::Draw(bool gameplayActive) const
{
  if (mode == ClientMenuMode::None) return;

  DrawMenuBackdrop(gameplayActive);
  Rectangle panel = BuildPanelBounds();
  DrawRectangleRounded(panel, 0.08f, 8, Fade(BLACK, 0.88f));
  DrawRectangleLinesEx(panel, 2.0f, Fade(RAYWHITE, 0.9f));

  switch (mode)
  {
    case ClientMenuMode::Main: DrawMainMenu(); break;
    case ClientMenuMode::Play: DrawPlayMenu(); break;
    case ClientMenuMode::Options: DrawOptionsMenu(); break;
    case ClientMenuMode::Credits: DrawCreditsMenu(); break;
    case ClientMenuMode::Pause: DrawPauseMenu(); break;
    case ClientMenuMode::None: break;
  }

  if (!statusMessage.empty())
  {
    DrawWrappedText(statusMessage, panel.x + kPanelPadding, panel.y + panel.height - 120.0f, panel.width - kPanelPadding * 2.0f, 18, 4.0f, Fade(RAYWHITE, 0.78f));
  }
}

void ClientMenu::DrawScreenTitle(const char* title, const char* subtitle) const
{
  Rectangle panel = BuildPanelBounds();
  DrawText(title, (int)panel.x + kPanelPadding, (int)panel.y + 24, 38, WHITE);
  DrawWrappedText(subtitle, panel.x + kPanelPadding, panel.y + 72.0f, panel.width - kPanelPadding * 2.0f, 20, 2.0f, Fade(RAYWHITE, 0.78f));
}

void ClientMenu::DrawButton(const Button& button, bool primary) const
{
  const bool hovered = CheckCollisionPointRec(GetMousePosition(), button.bounds);
  const unsigned char alpha = (unsigned char)(hovered ? 255 : 225);
  const Color fillColor = primary ? Color{53, 118, 255, alpha} : Color{32, 38, 56, alpha};
  const Color borderColor = primary ? Fade(SKYBLUE, 0.95f) : Fade(RAYWHITE, 0.5f);
  DrawRectangleRounded(button.bounds, 0.2f, 8, fillColor);
  DrawRectangleLinesEx(button.bounds, primary ? 2.0f : 1.5f, borderColor);
  const int fontSize = 24;
  const int textWidth = MeasureText(button.label.c_str(), fontSize);
  DrawText(
      button.label.c_str(),
      (int)(button.bounds.x + (button.bounds.width - (float)textWidth) * 0.5f),
      (int)(button.bounds.y + (button.bounds.height - (float)fontSize) * 0.5f - 2.0f),
      fontSize,
      GetMenuTextColor(hovered));
}

void ClientMenu::DrawMainMenu() const
{
  Rectangle panel = BuildPanelBounds();
  DrawScreenTitle("OpenRealm", "Client node main menu");
  DrawButton(BuildCenteredButton(panel.y + 140.0f, "Play"), true);
  DrawButton(BuildCenteredButton(panel.y + 208.0f, "Options"), false);
  DrawButton(BuildCenteredButton(panel.y + 276.0f, "Credits"), false);
  DrawButton(BuildCenteredButton(panel.y + 344.0f, "Quit"), false);
}

void ClientMenu::DrawRealmSummary(float top) const
{
  Rectangle panel = BuildPanelBounds();
  const int left = (int)panel.x + 28;
  DrawText("Current realm", left, (int)top, 18, Fade(RAYWHITE, 0.75f));
  DrawText(selectedRealm.realmName.c_str(), left, (int)top + 24, 28, WHITE);
  DrawText(FitTextToWidth(config.selectedRealm, 18, panel.width - 56.0f).c_str(), left, (int)top + 58, 18, Fade(RAYWHITE, 0.82f));
}

void ClientMenu::DrawStepperRow(float top, const std::string& label, const std::string& valueText) const
{
  Rectangle panel = BuildPanelBounds();
  const StepperRowLayout layout = BuildStepperRowLayout(panel, top);
  const bool leftHovered = CheckCollisionPointRec(GetMousePosition(), layout.leftButton);
  const bool rightHovered = CheckCollisionPointRec(GetMousePosition(), layout.rightButton);
  const std::string fittedLabel = FitTextToWidth(label, 22, layout.labelBounds.width);
  const std::string fittedValue = FitTextToWidth(valueText, 20, layout.valueBounds.width - 12.0f);

  DrawText(fittedLabel.c_str(), (int)layout.labelBounds.x, (int)layout.labelBounds.y + 10, 22, WHITE);
  DrawRectangleRounded(layout.leftButton, 0.18f, 6, Fade(DARKGRAY, leftHovered ? 0.95f : 0.8f));
  DrawRectangleRounded(layout.rightButton, 0.18f, 6, Fade(DARKGRAY, rightHovered ? 0.95f : 0.8f));
  DrawText("<", (int)layout.leftButton.x + 13, (int)layout.leftButton.y + 8, 24, WHITE);
  DrawText(">", (int)layout.rightButton.x + 13, (int)layout.rightButton.y + 8, 24, WHITE);
  DrawRectangleRounded(layout.valueBounds, 0.18f, 6, Fade(BLACK, 0.45f));
  DrawRectangleLinesEx(layout.valueBounds, 1.0f, Fade(RAYWHITE, 0.35f));
  const int valueWidth = MeasureText(fittedValue.c_str(), 20);
  DrawText(fittedValue.c_str(), (int)(layout.valueBounds.x + (layout.valueBounds.width - (float)valueWidth) * 0.5f), (int)layout.valueBounds.y + 10, 20, Fade(RAYWHITE, 0.78f));
}

void ClientMenu::DrawToggleRow(float top, const std::string& label, bool value) const
{
  Rectangle panel = BuildPanelBounds();
  const Rectangle buttonBounds = {panel.x + panel.width - 168.0f, top, 120.0f, 42.0f};
  const bool hovered = CheckCollisionPointRec(GetMousePosition(), buttonBounds);
  DrawText(FitTextToWidth(label, 22, panel.width - 228.0f).c_str(), (int)panel.x + 28, (int)top + 10, 22, WHITE);
  const unsigned char alpha = (unsigned char)(hovered ? 255 : 225);
  DrawRectangleRounded(buttonBounds, 0.18f, 6, value ? Color{53, 118, 255, alpha} : Fade(DARKGRAY, hovered ? 0.95f : 0.8f));
  DrawRectangleLinesEx(buttonBounds, 1.5f, Fade(RAYWHITE, 0.45f));
  const char* text = value ? "On" : "Off";
  const int textWidth = MeasureText(text, 22);
  DrawText(text, (int)(buttonBounds.x + (buttonBounds.width - (float)textWidth) * 0.5f), (int)buttonBounds.y + 10, 22, WHITE);
}

void ClientMenu::DrawPlayMenu() const
{
  Rectangle panel = BuildPanelBounds();
  DrawScreenTitle("Play", "Choose the realm, jump node, and explicit world join target.");
  DrawRealmSummary(panel.y + 112.0f);

  DrawStepperRow(panel.y + kPlayRowTop, "Realm", selectedRealm.realmName);

  const std::string jumpNodeLabel = selectedRealm.jumpNodes.empty()
                                        ? std::string("No jump nodes available")
                                        : selectedRealm.jumpNodes[(size_t)config.jumpNodeIndex].label;
  DrawStepperRow(panel.y + kPlayRowTop + kRowStep, "Jump node", jumpNodeLabel);

  char targetXText[32] = {};
  std::snprintf(targetXText, sizeof(targetXText), "%.0f", config.joinTargetPosition.x);
  DrawStepperRow(panel.y + kPlayRowTop + kRowStep * 2.0f, "Join target X", targetXText);

  char targetYText[32] = {};
  std::snprintf(targetYText, sizeof(targetYText), "%.0f", config.joinTargetPosition.y);
  DrawStepperRow(panel.y + kPlayRowTop + kRowStep * 3.0f, "Join target Y", targetYText);

  char targetZText[32] = {};
  std::snprintf(targetZText, sizeof(targetZText), "%.0f", config.joinTargetPosition.z);
  DrawStepperRow(panel.y + kPlayRowTop + kRowStep * 4.0f, "Join target Z", targetZText);

  const std::string jumpNodeDetails = selectedRealm.jumpNodes.empty()
                                          ? std::string("No host/port available for the selected realm.")
                                          : selectedRealm.jumpNodes[(size_t)config.jumpNodeIndex].host + ":" +
                                                std::to_string(selectedRealm.jumpNodes[(size_t)config.jumpNodeIndex].port);
  DrawWrappedText(jumpNodeDetails, panel.x + kPanelPadding, panel.y + 468.0f, panel.width - kPanelPadding * 2.0f, 18, 4.0f, Fade(RAYWHITE, 0.78f));
  DrawWrappedText(
      "The client now joins the realm around the explicit target position. Jump nodes only bootstrap topology discovery and no longer define the in-world spawn point.",
      panel.x + kPanelPadding,
      panel.y + 496.0f,
      panel.width - kPanelPadding * 2.0f,
      18,
      4.0f,
      Fade(RAYWHITE, 0.78f));

  DrawButton(BuildFooterButton(panel.x + panel.width - 284.0f, panel.y + panel.height - 72.0f, 124.0f, "Play"), true);
  DrawButton(BuildFooterButton(panel.x + panel.width - 144.0f, panel.y + panel.height - 72.0f, 116.0f, "Back"), false);
}

void ClientMenu::DrawOptionsMenu() const
{
  Rectangle panel = BuildPanelBounds();
  DrawScreenTitle("Options", "Configure client-side controls and presentation.");

  char volumeText[32] = {};
  std::snprintf(volumeText, sizeof(volumeText), "%d%%", (int)(config.masterVolume * 100.0f + 0.5f));
  DrawStepperRow(panel.y + kOptionsRowTop, "Master volume", volumeText);

  char sensitivityText[32] = {};
  std::snprintf(sensitivityText, sizeof(sensitivityText), "%.2fx", config.mouseSensitivity);
  DrawStepperRow(panel.y + kOptionsRowTop + kRowStep, "Mouse sensitivity", sensitivityText);
  DrawToggleRow(panel.y + kOptionsRowTop + kRowStep * 2.0f, "Invert mouse Y", config.invertMouseY);
  DrawToggleRow(panel.y + kOptionsRowTop + kRowStep * 3.0f, "Show FPS", config.showFps);
  DrawButton(BuildBackButton(), false);
}

void ClientMenu::DrawCreditsMenu() const
{
  Rectangle panel = BuildPanelBounds();
  DrawScreenTitle("Credits", "OpenRealm prototype client");
  DrawText("Project: Christian Luppi", (int)panel.x + 28, (int)panel.y + 150, 26, WHITE);
  DrawText("Rendering / audio / input: raylib", (int)panel.x + 28, (int)panel.y + 198, 22, Fade(RAYWHITE, 0.85f));
  DrawText("Profiling: Tracy", (int)panel.x + 28, (int)panel.y + 232, 22, Fade(RAYWHITE, 0.85f));
  DrawWrappedText(
      "OpenRealm currently focuses on the local engine/client foundation while the wider decentralized runtime grows around it.",
      panel.x + kPanelPadding,
      panel.y + 296.0f,
      panel.width - kPanelPadding * 2.0f,
      20,
      4.0f,
      Fade(RAYWHITE, 0.78f));
  DrawButton(BuildBackButton(), false);
}

void ClientMenu::DrawPauseMenu() const
{
  Rectangle panel = BuildPanelBounds();
  DrawScreenTitle("Paused", "Resume play, adjust options, or return to the main menu.");
  DrawButton(BuildCenteredButton(panel.y + 140.0f, "Resume"), true);
  DrawButton(BuildCenteredButton(panel.y + 208.0f, "Options"), false);
  DrawButton(BuildCenteredButton(panel.y + 276.0f, "Return to main menu"), false);
  DrawButton(BuildCenteredButton(panel.y + 344.0f, "Quit"), false);
}

void ClientMenu::DrawMenuBackdrop(bool gameplayActive) const
{
  if (!gameplayActive)
  {
    ClearBackground(Color{12, 16, 28, 255});
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), Color{18, 28, 48, 255}, Color{7, 10, 18, 255});
    return;
  }

  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.45f));
}
