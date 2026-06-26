#pragma once

#include <string>
#include <unordered_map>

#include "../world/WorldConfig.h"

class AssetManager
{
 public:
  AssetManager() = default;
  ~AssetManager();

  AssetManager(const AssetManager&) = delete;
  AssetManager& operator=(const AssetManager&) = delete;

  void Shutdown();
  Texture2D FetchTexture(const char* name);
  Shader FetchShader(const char* fragmentName);
  Shader FetchShader(const char* vertexName, const char* fragmentName);
  int FetchShaderLocation(Shader shader, const char* uniformName);
  Sound FetchSound(const char* name);

 private:
  std::unordered_map<std::string, Texture2D> textures;
  std::unordered_map<std::string, Shader> shaders;
  std::unordered_map<std::string, int> shaderLocations;
  std::unordered_map<std::string, Sound> sounds;
};
