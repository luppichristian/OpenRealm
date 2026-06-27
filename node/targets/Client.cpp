#include "../TaskManager.h"
#include "../client/Game.h"

int main()
{
  TaskManager taskManager = {};
  Game game = {};
  return game.Run(taskManager);
}
