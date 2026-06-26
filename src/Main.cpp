#include "client/Game.h"

#include <memory>

int main()
{
  auto game = std::make_unique<Game>();
  return game->Run();
}
