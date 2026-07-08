#include <csignal>

#include "App.h"
#include "Process.h"

int main(int argc, char** argv)
{
  std::signal(SIGINT, HandleStopSignal);
  std::signal(SIGTERM, HandleStopSignal);
  return RunApp(argc, argv);
}
