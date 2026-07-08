#pragma once

#include <string>

#include "LauncherTypes.h"

void PrintUsage();
bool ParseArguments(int argc, char** argv, LaunchOptions* options, std::string* errorMessage);
