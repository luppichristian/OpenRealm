#pragma once

#include "AssetManager.h"

class SoundPlayer
{
 public:
  SoundPlayer() = default;
  ~SoundPlayer();

  SoundPlayer(const SoundPlayer&) = delete;
  SoundPlayer& operator=(const SoundPlayer&) = delete;

  void Initialize(AssetManager& assetManager);
  void Shutdown();
  bool IsInitialized() const;
  void PlayFootstep();
  void PlayLanding();

 private:
  static void ConfigureSoundIfValid(Sound sound, float volume, float pitch);
  static void PlaySoundFromPool(Sound* sounds, int soundCount, int* nextIndex);

  bool initialized = false;
  Sound footstepSource = {};
  Sound footstepVoices[FOOTSTEP_VOICE_COUNT] = {};
  Sound landingVoices[LANDING_VOICE_COUNT] = {};
  int footstepVoiceIndex = 0;
  int landingVoiceIndex = 0;
};
