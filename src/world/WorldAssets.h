#pragma once

#include "WorldConfig.h"

class WorldAssets
{
 public:
  WorldAssets() = default;
  ~WorldAssets();

  WorldAssets(const WorldAssets&) = delete;
  WorldAssets& operator=(const WorldAssets&) = delete;

  void Initialize();
  void Shutdown();
  bool IsInitialized() const;
  void PlayFootstep();
  void PlayLanding();
  void UpdateShaders(Vector3 cameraPosition, Vector3 cameraForward, Vector3 cameraRight, Vector3 cameraUp) const;
  Shader GetFloorShader() const;
  Shader GetVoxelShader() const;

 private:
  static void ConfigureSoundIfValid(Sound sound, float volume, float pitch);
  static void PlaySoundFromPool(Sound* sounds, int soundCount, int* nextIndex);

  bool initialized = false;
  Shader floorShader = {};
  Shader voxelShader = {};
  Sound footstepSource = {};
  Sound footstepVoices[FOOTSTEP_VOICE_COUNT] = {};
  Sound landingVoices[LANDING_VOICE_COUNT] = {};
  int footstepVoiceIndex = 0;
  int landingVoiceIndex = 0;
  int floorCameraPositionLoc = 0;
  int floorCameraForwardLoc = 0;
  int floorCameraRightLoc = 0;
  int floorCameraUpLoc = 0;
  int floorResolutionLoc = 0;
  int floorVerticalFovLoc = 0;
  int floorAspectRatioLoc = 0;
  int floorTimeSecondsLoc = 0;
  int floorBlockGridSizeLoc = 0;
  int floorChunkGridSizeLoc = 0;
  int voxelCameraPositionLoc = 0;
  int voxelBlockGridSizeLoc = 0;
  int voxelTimeSecondsLoc = 0;
};
