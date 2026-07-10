#include "ClientLaunchArgs.h"

#include <cstdlib>
#include <iostream>

static bool ParseInt(const std::string& text, int* value)
{
  if (value == nullptr) return false;
  char* end = nullptr;
  const long parsed = std::strtol(text.c_str(), &end, 10);
  if (end == text.c_str() || (end != nullptr && *end != '\0')) return false;
  *value = (int)parsed;
  return true;
}

static bool ParseFloat(const std::string& text, float* value)
{
  if (value == nullptr) return false;
  char* end = nullptr;
  const float parsed = std::strtof(text.c_str(), &end);
  if (end == text.c_str() || (end != nullptr && *end != '\0')) return false;
  *value = parsed;
  return true;
}

void PrintClientUsage()
{
  std::cout << "OpenRealm client\n";
  std::cout << "Usage:\n";
  std::cout << "  openrealm-client [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  --config <path>                Config file to load (default: config.json)\n";
  std::cout << "  --realm-dir <path>             Realm directory override for startup\n";
  std::cout << "  --jump-node-index <index>      Jump node index to use for startup (default: 0)\n";
  std::cout << "  --join-target-x <value>        Startup join target X coordinate (default: 0)\n";
  std::cout << "  --join-target-y <value>        Startup join target Y coordinate (default: 0)\n";
  std::cout << "  --join-target-z <value>        Startup join target Z coordinate (default: 0)\n";
  std::cout << "  --auto-play                    Start gameplay immediately with the selected startup options\n";
  std::cout << "  --help                         Show this help text\n";
}

bool ParseClientLaunchArgs(int argc, char** argv, ClientLaunchArgs* options, std::string* errorMessage)
{
  if (options == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "client launch options output was null";
    return false;
  }

  for (int i = 1; i < argc; ++i)
  {
    const std::string argument = argv[i] == nullptr ? std::string() : std::string(argv[i]);
    if (argument == "--help")
    {
      PrintClientUsage();
      return false;
    }

    auto requireValue = [&](const char* label, std::string* value)
    {
      if (i + 1 >= argc)
      {
        if (errorMessage != nullptr) *errorMessage = std::string("missing value for ") + label;
        return false;
      }
      *value = argv[++i] == nullptr ? std::string() : std::string(argv[i]);
      return true;
    };

    std::string value = {};
    if (argument == "--config")
    {
      if (!requireValue("--config", &value)) return false;
      options->configPath = value;
      continue;
    }

    if (argument == "--realm-dir")
    {
      if (!requireValue("--realm-dir", &value)) return false;
      options->realmDirectory = value;
      continue;
    }

    if (argument == "--jump-node-index")
    {
      if (!requireValue("--jump-node-index", &value) || !ParseInt(value, &options->jumpNodeIndex))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--jump-node-index must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--join-target-x")
    {
      if (!requireValue("--join-target-x", &value) || !ParseFloat(value, &options->joinTargetPosition.x))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--join-target-x must be a number";
        return false;
      }
      continue;
    }

    if (argument == "--join-target-y")
    {
      if (!requireValue("--join-target-y", &value) || !ParseFloat(value, &options->joinTargetPosition.y))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--join-target-y must be a number";
        return false;
      }
      continue;
    }

    if (argument == "--join-target-z")
    {
      if (!requireValue("--join-target-z", &value) || !ParseFloat(value, &options->joinTargetPosition.z))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--join-target-z must be a number";
        return false;
      }
      continue;
    }

    if (argument == "--auto-play")
    {
      options->autoPlay = true;
      continue;
    }

    if (errorMessage != nullptr) *errorMessage = "unknown argument: " + argument;
    return false;
  }

  if (options->jumpNodeIndex < 0)
  {
    if (errorMessage != nullptr) *errorMessage = "--jump-node-index cannot be negative";
    return false;
  }

  return true;
}
