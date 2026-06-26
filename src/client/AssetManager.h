#pragma once

#include <string>
#include <unordered_map>

#include "../Utils.h"

class AssetManager : public NonCopyable
{
 public:
  AssetManager() = default;
  ~AssetManager();

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
