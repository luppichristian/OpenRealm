#pragma once

#include <string>

enum class NodeCliRole
{
  Relay,
  Simulator,
};

struct NodeCliOptions
{
  NodeCliRole role = NodeCliRole::Relay;
  std::string configPath = "config.json";
};

bool ShouldRunNodeCli(int argc, char** argv);
bool RunNodeCli(const NodeCliOptions& options);
