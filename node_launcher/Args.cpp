#include "Args.h"

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

void PrintUsage()
{
  std::cout << "OpenRealm node launcher\n";
  std::cout << "Usage:\n";
  std::cout << "  openrealm-node-launcher --realm <name-or-path> [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  --relays <count>               Relay nodes to launch (default: 1)\n";
  std::cout << "  --simulators <count>           Simulator nodes to launch (default: 0)\n";
  std::cout << "  --clients <count>              Client nodes to launch (default: 0)\n";
  std::cout << "  --config <path>                Base config.json to clone (default: config.json)\n";
  std::cout << "  --relay-base-port <port>       First relay UDP port (default: 46001)\n";
  std::cout << "  --sim-base-port <port>         First simulator UDP port (default: 46101)\n";
  std::cout << "  --client-base-port <port>      First client UDP port (default: 46201)\n";
  std::cout << "  --relay-ticks <count>          Relay service ticks, 0 = infinite (default: 0)\n";
  std::cout << "  --sim-frames <count>           Simulator frames, 0 = infinite (default: 0)\n";
  std::cout << "  --sim-sleep-ms <ms>            Simulator frame sleep (default: 16)\n";
  std::cout << "  --sim-frame-time <seconds>     Simulator timestep (default: 0.0166667)\n";
  std::cout << "  --launch-delay-ms <ms>         Delay between child launches (default: 250)\n";
  std::cout << "  --run-seconds <seconds>        Auto-stop after N seconds, 0 = wait for Ctrl+C\n";
  std::cout << "  --emit-place-event             Let the first simulator emit the startup test place event\n";
  std::cout << "  --help                         Show this help text\n";
}

bool ParseArguments(int argc, char** argv, LaunchOptions* options, std::string* errorMessage)
{
  if (options == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "launch options output was null";
    return false;
  }

  for (int i = 1; i < argc; ++i)
  {
    const std::string argument = argv[i] == nullptr ? std::string() : std::string(argv[i]);
    if (argument == "--help")
    {
      PrintUsage();
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
    if (argument == "--realm")
    {
      if (!requireValue("--realm", &value)) return false;
      options->realmArgument = value;
      continue;
    }

    if (argument == "--config")
    {
      if (!requireValue("--config", &value)) return false;
      options->baseConfigPath = value;
      continue;
    }

    if (argument == "--relays")
    {
      if (!requireValue("--relays", &value) || !ParseInt(value, &options->relayCount))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--relays must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--simulators")
    {
      if (!requireValue("--simulators", &value) || !ParseInt(value, &options->simulatorCount))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--simulators must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--clients")
    {
      if (!requireValue("--clients", &value) || !ParseInt(value, &options->clientCount))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--clients must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--relay-base-port")
    {
      if (!requireValue("--relay-base-port", &value) || !ParseInt(value, &options->relayBasePort))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--relay-base-port must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--sim-base-port")
    {
      if (!requireValue("--sim-base-port", &value) || !ParseInt(value, &options->simulatorBasePort))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--sim-base-port must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--client-base-port")
    {
      if (!requireValue("--client-base-port", &value) || !ParseInt(value, &options->clientBasePort))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--client-base-port must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--relay-ticks")
    {
      if (!requireValue("--relay-ticks", &value) || !ParseInt(value, &options->relayTicks))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--relay-ticks must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--sim-frames")
    {
      if (!requireValue("--sim-frames", &value) || !ParseInt(value, &options->simulatorFrames))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--sim-frames must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--sim-sleep-ms")
    {
      if (!requireValue("--sim-sleep-ms", &value) || !ParseInt(value, &options->simulatorSleepMs))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--sim-sleep-ms must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--sim-frame-time")
    {
      if (!requireValue("--sim-frame-time", &value) || !ParseFloat(value, &options->simulatorFrameTime))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--sim-frame-time must be a number";
        return false;
      }
      continue;
    }

    if (argument == "--launch-delay-ms")
    {
      if (!requireValue("--launch-delay-ms", &value) || !ParseInt(value, &options->launchDelayMs))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--launch-delay-ms must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--run-seconds")
    {
      if (!requireValue("--run-seconds", &value) || !ParseInt(value, &options->runSeconds))
      {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "--run-seconds must be an integer";
        return false;
      }
      continue;
    }

    if (argument == "--emit-place-event")
    {
      options->emitPlaceEvent = true;
      continue;
    }

    if (errorMessage != nullptr) *errorMessage = "unknown argument: " + argument;
    return false;
  }

  if (options->realmArgument.empty())
  {
    if (errorMessage != nullptr) *errorMessage = "--realm is required";
    return false;
  }

  if (options->relayCount < 0 || options->simulatorCount < 0 || options->clientCount < 0)
  {
    if (errorMessage != nullptr) *errorMessage = "node counts cannot be negative";
    return false;
  }

  if (options->relayCount == 0 && options->simulatorCount == 0 && options->clientCount == 0)
  {
    if (errorMessage != nullptr) *errorMessage = "at least one relay, simulator, or client must be launched";
    return false;
  }

  if (options->relayBasePort < 1 || options->relayBasePort > 65535 || options->simulatorBasePort < 1 || options->simulatorBasePort > 65535 || options->clientBasePort < 1 || options->clientBasePort > 65535)
  {
    if (errorMessage != nullptr) *errorMessage = "base ports must be between 1 and 65535";
    return false;
  }

  if (options->launchDelayMs < 0 || options->runSeconds < 0)
  {
    if (errorMessage != nullptr) *errorMessage = "delay and runtime cannot be negative";
    return false;
  }

  return true;
}
