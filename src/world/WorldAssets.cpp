#include "WorldAssets.h"

WorldAssets::~WorldAssets()
{
  Shutdown();
}

void WorldAssets::ConfigureSoundIfValid(Sound sound, float volume, float pitch)
{
  if (!IsSoundValid(sound)) return;
  SetSoundVolume(sound, volume);
  SetSoundPitch(sound, pitch);
}

void WorldAssets::PlaySoundFromPool(Sound* sounds, int soundCount, int* nextIndex)
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

void WorldAssets::Initialize()
{
  if (initialized) return;

  floorShader = LoadShader(0, "assets/shaders/infinite_floor.fs");
  voxelShader = LoadShader("assets/shaders/voxel_chunk.vs", "assets/shaders/voxel_chunk.fs");
  footstepSource = LoadSound("assets/sounds/footstep.wav");
  floorCameraPositionLoc = GetShaderLocation(floorShader, "cameraPosition");
  floorCameraForwardLoc = GetShaderLocation(floorShader, "cameraForward");
  floorCameraRightLoc = GetShaderLocation(floorShader, "cameraRight");
  floorCameraUpLoc = GetShaderLocation(floorShader, "cameraUp");
  floorResolutionLoc = GetShaderLocation(floorShader, "resolution");
  floorVerticalFovLoc = GetShaderLocation(floorShader, "verticalFovRadians");
  floorAspectRatioLoc = GetShaderLocation(floorShader, "aspectRatio");
  floorTimeSecondsLoc = GetShaderLocation(floorShader, "timeSeconds");
  floorBlockGridSizeLoc = GetShaderLocation(floorShader, "blockGridSize");
  floorChunkGridSizeLoc = GetShaderLocation(floorShader, "chunkGridSize");
  voxelCameraPositionLoc = GetShaderLocation(voxelShader, "cameraPosition");
  voxelBlockGridSizeLoc = GetShaderLocation(voxelShader, "blockGridSize");
  voxelTimeSecondsLoc = GetShaderLocation(voxelShader, "timeSeconds");

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

void WorldAssets::Shutdown()
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

  if (IsSoundValid(footstepSource)) UnloadSound(footstepSource);
  UnloadShader(voxelShader);
  UnloadShader(floorShader);
  initialized = false;
  floorShader = {};
  voxelShader = {};
  footstepSource = {};
  memset(footstepVoices, 0, sizeof(footstepVoices));
  memset(landingVoices, 0, sizeof(landingVoices));
  footstepVoiceIndex = 0;
  landingVoiceIndex = 0;
  floorCameraPositionLoc = 0;
  floorCameraForwardLoc = 0;
  floorCameraRightLoc = 0;
  floorCameraUpLoc = 0;
  floorResolutionLoc = 0;
  floorVerticalFovLoc = 0;
  floorAspectRatioLoc = 0;
  floorTimeSecondsLoc = 0;
  floorBlockGridSizeLoc = 0;
  floorChunkGridSizeLoc = 0;
  voxelCameraPositionLoc = 0;
  voxelBlockGridSizeLoc = 0;
  voxelTimeSecondsLoc = 0;
}

bool WorldAssets::IsInitialized() const
{
  return initialized;
}

void WorldAssets::PlayFootstep()
{
  PlaySoundFromPool(footstepVoices, FOOTSTEP_VOICE_COUNT, &footstepVoiceIndex);
}

void WorldAssets::PlayLanding()
{
  PlaySoundFromPool(landingVoices, LANDING_VOICE_COUNT, &landingVoiceIndex);
}

void WorldAssets::UpdateShaders(Vector3 cameraPosition, Vector3 cameraForward, Vector3 cameraRight, Vector3 cameraUp) const
{
  Vector2 resolution = {(float)GetScreenWidth(), (float)GetScreenHeight()};
  float aspectRatio = resolution.x / resolution.y;
  float timeSeconds = (float)GetTime();
  float chunkGridSize = BLOCK_GRID_SIZE * (float)CHUNK_SIZE_XZ;
  float verticalFovRadians = VERTICAL_FOV_RADIANS;
  float blockGridSize = BLOCK_GRID_SIZE;

  SetShaderValue(floorShader, floorCameraPositionLoc, &cameraPosition, SHADER_UNIFORM_VEC3);
  SetShaderValue(floorShader, floorCameraForwardLoc, &cameraForward, SHADER_UNIFORM_VEC3);
  SetShaderValue(floorShader, floorCameraRightLoc, &cameraRight, SHADER_UNIFORM_VEC3);
  SetShaderValue(floorShader, floorCameraUpLoc, &cameraUp, SHADER_UNIFORM_VEC3);
  SetShaderValue(floorShader, floorResolutionLoc, &resolution, SHADER_UNIFORM_VEC2);
  SetShaderValue(floorShader, floorVerticalFovLoc, &verticalFovRadians, SHADER_UNIFORM_FLOAT);
  SetShaderValue(floorShader, floorAspectRatioLoc, &aspectRatio, SHADER_UNIFORM_FLOAT);
  SetShaderValue(floorShader, floorTimeSecondsLoc, &timeSeconds, SHADER_UNIFORM_FLOAT);
  SetShaderValue(floorShader, floorBlockGridSizeLoc, &blockGridSize, SHADER_UNIFORM_FLOAT);
  SetShaderValue(floorShader, floorChunkGridSizeLoc, &chunkGridSize, SHADER_UNIFORM_FLOAT);
  SetShaderValue(voxelShader, voxelCameraPositionLoc, &cameraPosition, SHADER_UNIFORM_VEC3);
  SetShaderValue(voxelShader, voxelBlockGridSizeLoc, &blockGridSize, SHADER_UNIFORM_FLOAT);
  SetShaderValue(voxelShader, voxelTimeSecondsLoc, &timeSeconds, SHADER_UNIFORM_FLOAT);
}

Shader WorldAssets::GetFloorShader() const
{
  return floorShader;
}

Shader WorldAssets::GetVoxelShader() const
{
  return voxelShader;
}
