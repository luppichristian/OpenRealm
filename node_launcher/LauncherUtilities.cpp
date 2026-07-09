#include "LauncherUtilities.h"

#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

std::string Trim(const std::string& value)
{
  size_t begin = 0;
  while (begin < value.size() && std::isspace((unsigned char)value[begin])) begin += 1;

  size_t end = value.size();
  while (end > begin && std::isspace((unsigned char)value[end - 1])) end -= 1;
  return value.substr(begin, end - begin);
}

std::string FormatPath(const std::filesystem::path& path)
{
  return path.generic_string();
}

std::string BuildTimestampString()
{
  const auto now = std::chrono::system_clock::now();
  const std::time_t rawNow = std::chrono::system_clock::to_time_t(now);
  std::tm timeInfo = {};
#if defined(_WIN32)
  localtime_s(&timeInfo, &rawNow);
#else
  localtime_r(&rawNow, &timeInfo);
#endif

  std::ostringstream stream = {};
  stream << std::put_time(&timeInfo, "%Y%m%d-%H%M%S");
  return stream.str();
}

std::pair<int, int> BuildSimulatorInterestCoords(int index)
{
  return {index, 0};
}

std::filesystem::path AppendExecutableSuffix(const std::filesystem::path& path)
{
#if defined(_WIN32)
  return path.string().ends_with(".exe") ? path : std::filesystem::path(path.string() + ".exe");
#else
  return path;
#endif
}

SessionPaths BuildSessionPaths(const std::filesystem::path& repoRoot)
{
  SessionPaths paths = {};
  const std::filesystem::path sessionRoot = repoRoot / "build" / "node_launcher";
  const std::string timestamp = BuildTimestampString();
  paths.sessionDir = sessionRoot / timestamp;
  paths.realmDir = paths.sessionDir / "realm";
  return paths;
}

std::filesystem::path GetExecutableDirectory(const char* argv0)
{
  std::error_code error = {};
  std::filesystem::path executablePath = std::filesystem::absolute(argv0 == nullptr ? "" : argv0, error);
  if (error) executablePath = argv0 == nullptr ? std::filesystem::path() : std::filesystem::path(argv0);
  return executablePath.parent_path();
}
