#include <csignal>
#include <iostream>

#include "../TaskManager.h"
#include "../client/ClientLaunchArgs.h"
#include "../client/Game.h"

int main(int argc, char** argv)
{
  ClientLaunchArgs launchArgs = {};
  std::string error = {};
  if (!ParseClientLaunchArgs(argc, argv, &launchArgs, &error))
  {
    if (!error.empty()) std::cout << "OpenRealm client\n- error: " << error << "\n";
    return error.empty() ? 0 : 1;
  }

  static TaskManager taskManager = {};
  static Game game = {};
  return game.Run(taskManager, launchArgs);
}
