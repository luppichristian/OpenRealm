#include "../TaskManager.h"
#include "../client/Game.h"

int main()
{
  static TaskManager taskManager = {};
  static Game game = {};
  return game.Run(taskManager);
}
