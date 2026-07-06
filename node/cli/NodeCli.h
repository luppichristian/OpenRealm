#pragma once

#include <string>
#include <vector>

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

struct NodeRuntimeViewLine
{
  std::string label = {};
  std::string value = {};
};

struct NodeRuntimeViewOptions
{
  NodeCliRole role = NodeCliRole::Relay;
  std::string title = {};
  std::string subtitle = {};
};

bool ShouldRunNodeCli(int argc, char** argv);
bool RunNodeCli(const NodeCliOptions& options);
bool BeginNodeRuntimeView(const NodeRuntimeViewOptions& options);
bool PumpNodeRuntimeView(const std::string& status, const std::vector<NodeRuntimeViewLine>& lines);
void EndNodeRuntimeView();
