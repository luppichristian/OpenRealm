#include "AssetManager.h"

#include "../Utils.h"

AssetManager::~AssetManager()
{
  Shutdown();
}

void AssetManager::Shutdown()
{
  for (auto& [path, texture] : textures)
  {
    if (IsTextureValid(texture)) UnloadTexture(texture);
  }

  for (auto& [path, shader] : shaders)
  {
    if (IsShaderValid(shader)) UnloadShader(shader);
  }

  for (auto& [path, sound] : sounds)
  {
    if (IsSoundValid(sound)) UnloadSound(sound);
  }

  textures.clear();
  shaders.clear();
  shaderLocations.clear();
  sounds.clear();
}

Texture2D AssetManager::FetchTexture(const char* name)
{
  std::string path = BuildAssetPath("textures", name);
  auto it = textures.find(path);
  if (it != textures.end()) return it->second;

  Texture2D texture = LoadTexture(path.c_str());
  textures.emplace(path, texture);
  return texture;
}

Shader AssetManager::FetchShader(const char* fragmentName)
{
  std::string fragmentPath = BuildAssetPath("shaders", fragmentName);
  auto it = shaders.find(fragmentPath);
  if (it != shaders.end()) return it->second;

  Shader shader = LoadShader(0, fragmentPath.c_str());
  shaders.emplace(fragmentPath, shader);
  return shader;
}

Shader AssetManager::FetchShader(const char* vertexName, const char* fragmentName)
{
  std::string vertexPath = BuildAssetPath("shaders", vertexName);
  std::string fragmentPath = BuildAssetPath("shaders", fragmentName);
  std::string key = vertexPath + "|" + fragmentPath;
  auto it = shaders.find(key);
  if (it != shaders.end()) return it->second;

  Shader shader = LoadShader(vertexPath.c_str(), fragmentPath.c_str());
  shaders.emplace(key, shader);
  return shader;
}

int AssetManager::FetchShaderLocation(Shader shader, const char* uniformName)
{
  std::string key = std::to_string(shader.id);
  key += "|";
  key += uniformName;
  auto it = shaderLocations.find(key);
  if (it != shaderLocations.end()) return it->second;

  int location = GetShaderLocation(shader, uniformName);
  shaderLocations.emplace(key, location);
  return location;
}

Sound AssetManager::FetchSound(const char* name)
{
  std::string path = BuildAssetPath("sounds", name);
  auto it = sounds.find(path);
  if (it != sounds.end()) return it->second;

  Sound sound = LoadSound(path.c_str());
  sounds.emplace(path, sound);
  return sound;
}
