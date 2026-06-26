#include "SoundPlayer.h"

SoundPlayer::~SoundPlayer()
{
  Shutdown();
}

void SoundPlayer::ConfigureSoundIfValid(Sound sound, float volume, float pitch)
{
  if (!IsSoundValid(sound)) return;
  SetSoundVolume(sound, volume);
  SetSoundPitch(sound, pitch);
}

void SoundPlayer::PlaySoundFromPool(Sound* sounds, int soundCount, int* nextIndex)
{
  if (soundCount <= 0) return;

  int startIndex = *nextIndex;
  for (int offset = 0; offset < soundCount; offset++)
  {
    int soundIndex = (startIndex + offset) % soundCount;
    if (!IsSoundValid(sounds[soundIndex])) continue;
    if (IsSoundPlaying(sounds[soundIndex])) continue;
    PlaySound(sounds[soundIndex]);
    *nextIndex = (soundIndex + 1) % soundCount;
    return;
  }

  if (IsSoundValid(sounds[startIndex]))
  {
    StopSound(sounds[startIndex]);
    PlaySound(sounds[startIndex]);
    *nextIndex = (startIndex + 1) % soundCount;
  }
}

void SoundPlayer::Initialize(AssetManager& assetManager)
{
  if (initialized) return;

  footstepSource = assetManager.FetchSound("footstep.wav");
  if (IsSoundValid(footstepSource))
  {
    for (int i = 0; i < FOOTSTEP_VOICE_COUNT; i++)
    {
      footstepVoices[i] = LoadSoundAlias(footstepSource);
      ConfigureSoundIfValid(footstepVoices[i], FOOTSTEP_VOLUME, FOOTSTEP_PITCH);
    }

    for (int i = 0; i < LANDING_VOICE_COUNT; i++)
    {
      landingVoices[i] = LoadSoundAlias(footstepSource);
      ConfigureSoundIfValid(landingVoices[i], LANDING_VOLUME, LANDING_PITCH);
    }
  }

  initialized = true;
}

void SoundPlayer::Shutdown()
{
  if (!initialized) return;

  for (int i = 0; i < FOOTSTEP_VOICE_COUNT; i++)
  {
    if (IsSoundValid(footstepVoices[i])) UnloadSoundAlias(footstepVoices[i]);
  }

  for (int i = 0; i < LANDING_VOICE_COUNT; i++)
  {
    if (IsSoundValid(landingVoices[i])) UnloadSoundAlias(landingVoices[i]);
  }

  footstepSource = {};
  memset(footstepVoices, 0, sizeof(footstepVoices));
  memset(landingVoices, 0, sizeof(landingVoices));
  footstepVoiceIndex = 0;
  landingVoiceIndex = 0;
  initialized = false;
}

bool SoundPlayer::IsInitialized() const
{
  return initialized;
}

void SoundPlayer::PlayFootstep()
{
  PlaySoundFromPool(footstepVoices, FOOTSTEP_VOICE_COUNT, &footstepVoiceIndex);
}

void SoundPlayer::PlayLanding()
{
  PlaySoundFromPool(landingVoices, LANDING_VOICE_COUNT, &landingVoiceIndex);
}
