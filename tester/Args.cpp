#include "Args.h"

#include <cstdlib>
#include <iostream>

namespace
{
bool ParseInt(const std::string& text, int* value)
{
  if (value == nullptr) return false;
  char* end = nullptr;
  const long parsed = std::strtol(text.c_str(), &end, 10);
  if (end == text.c_str() || (end != nullptr && *end != '\0')) return false;
  *value = (int)parsed;
  return true;
}

bool ParseFloat(const std::string& text, float* value)
{
  if (value == nullptr) return false;
  char* end = nullptr;
  const float parsed = std::strtof(text.c_str(), &end);
  if (end == text.c_str() || (end != nullptr && *end != '\0')) return false;
  *value = parsed;
  return true;
}
}

void PrintTesterUsage()
{
  std::cout << "OpenRealm realm tester\n";
  std::cout << "Usage:\n";
  std::cout << "  openrealm-realm-tester [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  --config <path>                Node config containing the runtime wallet (default: config.json)\n";
  std::cout << "  --realm-dir <path>             Realm directory to test (default: realms/test)\n";
  std::cout << "  --jump-node-index <index>      Jump node index from jump_nodes.json (default: 0)\n";
  std::cout << "  --bind-host <host>             Local UDP bind host (default: 127.0.0.1)\n";
  std::cout << "  --bind-port <port>             Local UDP bind port (default: 46301)\n";
  std::cout << "  --join-target-x <value>        Requested join target x (default: 0)\n";
  std::cout << "  --join-target-y <value>        Requested join target y (default: 0)\n";
  std::cout << "  --join-target-z <value>        Requested join target z (default: 0)\n";
  std::cout << "  --run-seconds <seconds>        Maximum probe duration (default: 8)\n";
  std::cout << "  --expect-topology-nodes <n>    Minimum nodes expected inside a topology snapshot (default: 1)\n";
  std::cout << "  --require-player-snapshot      Fail unless at least one player snapshot is observed\n";
  std::cout << "  --help                         Show this help text\n";
}

bool ParseTesterArguments(int argc, char** argv, TesterOptions* options, std::string* errorMessage)
{
  if (options == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "tester options output was null";
    return false;
  }

  for (int i = 1; i < argc; ++i)
  {
    const std::string argument = argv[i] == nullptr ? std::string() : std::string(argv[i]);
    if (argument == "--help")
    {
      PrintTesterUsage();
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
      int parsed = 0;
      if (!requireValue("--jump-node-index", &value) || !ParseInt(value, &parsed) || parsed < 0)
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--jump-node-index must be a non-negative integer";
        return false;
      }
      options->jumpNodeIndex = parsed;
      continue;
    }
    if (argument == "--bind-host")
    {
      if (!requireValue("--bind-host", &value)) return false;
      options->bindAddress.host = value;
      continue;
    }
    if (argument == "--bind-port")
    {
      int parsed = 0;
      if (!requireValue("--bind-port", &value) || !ParseInt(value, &parsed))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--bind-port must be an integer";
        return false;
      }
      options->bindAddress.port = parsed;
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
    if (argument == "--run-seconds")
    {
      int parsed = 0;
      if (!requireValue("--run-seconds", &value) || !ParseInt(value, &parsed) || parsed <= 0)
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--run-seconds must be a positive integer";
        return false;
      }
      options->runSeconds = (uint32_t)parsed;
      continue;
    }
    if (argument == "--expect-topology-nodes")
    {
      int parsed = 0;
      if (!requireValue("--expect-topology-nodes", &value) || !ParseInt(value, &parsed) || parsed < 1)
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--expect-topology-nodes must be a positive integer";
        return false;
      }
      options->expectMinTopologyNodes = (size_t)parsed;
      continue;
    }
    if (argument == "--require-player-snapshot")
    {
      options->requirePlayerSnapshot = true;
      continue;
    }

    if (errorMessage != nullptr) *errorMessage = "unknown argument: " + argument;
    return false;
  }

  if (options->bindAddress.host.empty())
  {
    if (errorMessage != nullptr) *errorMessage = "--bind-host cannot be empty";
    return false;
  }
  if (options->bindAddress.port < 1 || options->bindAddress.port > 65535)
  {
    if (errorMessage != nullptr) *errorMessage = "--bind-port must be between 1 and 65535";
    return false;
  }
  return true;
}
