#include "Platform.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

#include "LauncherUtilities.h"

#if defined(_WIN32)
#else
#  include <fcntl.h>
#  include <signal.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

static std::string QuoteCommandArgument(const std::string& value)
{
  bool needsQuotes = value.empty();
  for (char ch : value)
  {
    if (std::isspace((unsigned char)ch) || ch == '"')
    {
      needsQuotes = true;
      break;
    }
  }

  if (!needsQuotes) return value;

  std::string result = '"' + value + '"';
  size_t position = 1;
  while ((position = result.find('"', position)) != std::string::npos)
  {
    if (position + 1 == result.size() - 1) break;
    result.insert(position, "\\");
    position += 2;
  }
  return result;
}

static std::string BuildCommandLine(const std::vector<std::string>& arguments)
{
  std::ostringstream stream = {};
  for (size_t i = 0; i < arguments.size(); ++i)
  {
    if (i > 0) stream << ' ';
    stream << QuoteCommandArgument(arguments[i]);
  }
  return stream.str();
}

bool PollPlatformChildProcess(ChildProcess* child)
{
  if (child == nullptr || !child->running) return false;
#if defined(_WIN32)
  DWORD exitCode = STILL_ACTIVE;
  if (!GetExitCodeProcess(child->processHandle, &exitCode)) return false;
  if (exitCode == STILL_ACTIVE) return true;
  child->running = false;
  child->exitCode = (int)exitCode;
  return false;
#else
  int status = 0;
  const pid_t result = waitpid(child->processId, &status, WNOHANG);
  if (result == 0) return true;
  if (result < 0) return false;
  child->running = false;
  child->exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  return false;
#endif
}

void ClosePlatformChildProcess(ChildProcess* child)
{
  if (child == nullptr) return;
#if defined(_WIN32)
  if (child->threadHandle != nullptr)
  {
    CloseHandle(child->threadHandle);
    child->threadHandle = nullptr;
  }
  if (child->processHandle != nullptr)
  {
    CloseHandle(child->processHandle);
    child->processHandle = nullptr;
  }
#endif
}

void StopPlatformChildProcess(ChildProcess* child)
{
  if (child == nullptr || !child->running) return;
  child->stoppedByLauncher = true;
#if defined(_WIN32)
  TerminateProcess(child->processHandle, 1);
  WaitForSingleObject(child->processHandle, 5000);
  DWORD exitCode = 1;
  GetExitCodeProcess(child->processHandle, &exitCode);
  child->running = false;
  child->exitCode = (int)exitCode;
#else
  kill(child->processId, SIGTERM);
  int status = 0;
  waitpid(child->processId, &status, 0);
  child->running = false;
  child->exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

bool LaunchPlatformChildProcess(
    const std::filesystem::path& repoRoot,
    const std::vector<std::string>& arguments,
    ChildProcess* child,
    std::string* errorMessage)
{
  if (child == nullptr)
  {
    if (errorMessage != nullptr) *errorMessage = "child output was null";
    return false;
  }

#if defined(_WIN32)
  SECURITY_ATTRIBUTES securityAttributes = {};
  securityAttributes.nLength = sizeof(securityAttributes);
  securityAttributes.bInheritHandle = TRUE;

  HANDLE inputHandle = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &securityAttributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  HANDLE logHandle = CreateFileA(
      FormatPath(child->logPath).c_str(),
      GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      &securityAttributes,
      CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      nullptr);
  if (inputHandle == INVALID_HANDLE_VALUE || logHandle == INVALID_HANDLE_VALUE)
  {
    if (inputHandle != INVALID_HANDLE_VALUE) CloseHandle(inputHandle);
    if (logHandle != INVALID_HANDLE_VALUE) CloseHandle(logHandle);
    if (errorMessage != nullptr) *errorMessage = "failed to open child stdio handles for " + child->role;
    return false;
  }

  STARTUPINFOA startupInfo = {};
  startupInfo.cb = sizeof(startupInfo);
  startupInfo.dwFlags = STARTF_USESTDHANDLES;
  startupInfo.hStdInput = inputHandle;
  startupInfo.hStdOutput = logHandle;
  startupInfo.hStdError = logHandle;

  PROCESS_INFORMATION processInfo = {};
  std::string commandLine = BuildCommandLine(arguments);
  std::vector<char> commandBuffer(commandLine.begin(), commandLine.end());
  commandBuffer.push_back('\0');

  const BOOL started = CreateProcessA(
      nullptr,
      commandBuffer.data(),
      nullptr,
      nullptr,
      TRUE,
      CREATE_NO_WINDOW,
      nullptr,
      FormatPath(repoRoot).c_str(),
      &startupInfo,
      &processInfo);

  CloseHandle(inputHandle);
  CloseHandle(logHandle);

  if (!started)
  {
    if (errorMessage != nullptr) *errorMessage = "failed to launch " + child->role + " from " + FormatPath(child->executablePath);
    return false;
  }

  child->running = true;
  child->processHandle = processInfo.hProcess;
  child->threadHandle = processInfo.hThread;
  child->processId = processInfo.dwProcessId;
  return true;
#else
  const pid_t pid = fork();
  if (pid < 0)
  {
    if (errorMessage != nullptr) *errorMessage = "fork failed for " + child->role;
    return false;
  }

  if (pid == 0)
  {
    const int inputFd = open("/dev/null", O_RDONLY);
    const int logFd = open(child->logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (inputFd >= 0) dup2(inputFd, STDIN_FILENO);
    if (logFd >= 0)
    {
      dup2(logFd, STDOUT_FILENO);
      dup2(logFd, STDERR_FILENO);
    }
    chdir(repoRoot.c_str());

    std::vector<char*> execArgs = {};
    for (const std::string& argument : arguments)
    {
      execArgs.push_back(const_cast<char*>(argument.c_str()));
    }
    execArgs.push_back(nullptr);
    execvp(execArgs[0], execArgs.data());
    std::_Exit(127);
  }

  child->running = true;
  child->processId = pid;
  return true;
#endif
}
