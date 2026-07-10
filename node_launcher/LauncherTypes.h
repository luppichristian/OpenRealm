#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#  define NOMINMAX
#  include <windows.h>
#else
#  include <sys/types.h>
#endif

struct LaunchOptions
{
  std::filesystem::path repoRoot = {};
  std::filesystem::path baseConfigPath = "config.json";
  std::string realmArgument = {};
  int relayCount = 1;
  int simulatorCount = 0;
  int clientCount = 0;
  int relayBasePort = 46001;
  int simulatorBasePort = 46101;
  int clientBasePort = 46201;
  int relayTicks = 0;
  int simulatorFrames = 0;
  int simulatorSleepMs = 16;
  float simulatorFrameTime = 1.0f / 60.0f;
  int launchDelayMs = 250;
  int runSeconds = 0;
  bool emitPlaceEvent = false;
};

struct SessionPaths
{
  std::filesystem::path sessionDir = {};
  std::filesystem::path realmDir = {};
};

struct ChildProcess
{
  std::string role = {};
  int index = 0;
  std::filesystem::path executablePath = {};
  std::filesystem::path configPath = {};
  std::filesystem::path logPath = {};
  std::filesystem::path realmDir = {};
  int bindPort = 0;
  int jumpNodeIndex = 0;
  bool running = false;
  bool stoppedByLauncher = false;
  int exitCode = -1;
#if defined(_WIN32)
  HANDLE processHandle = nullptr;
  HANDLE threadHandle = nullptr;
  DWORD processId = 0;
#else
  pid_t processId = -1;
#endif
};
