// Minimal, portable helpers for running a tool and capturing its output.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#define TOOLS_POPEN _popen
#define TOOLS_PCLOSE _pclose
#else
#include <sys/wait.h>
#define TOOLS_POPEN popen
#define TOOLS_PCLOSE pclose
#endif

namespace tools {

inline std::string Quote(const std::string& s) { return "\"" + s + "\""; }

// Runs `command` through the platform shell, capturing stdout and stderr.
// Returns the process exit code (or -1 if it could not be started).
inline int Run(std::string command, std::string* output) {
  command += " 2>&1";
#ifdef _WIN32
  // cmd.exe /c strips the first and last quote when the line starts with one.
  command = "\"" + command + "\"";
#endif
  std::FILE* pipe = TOOLS_POPEN(command.c_str(), "r");
  if (pipe == nullptr) return -1;
  char buffer[4096];
  size_t n;
  while ((n = std::fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
    if (output != nullptr) output->append(buffer, n);
  }
  int status = TOOLS_PCLOSE(pipe);
#ifdef _WIN32
  return status;
#else
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

inline std::string TempDir() {
  const char* dir = std::getenv("TEST_TMPDIR");
  return dir != nullptr ? dir : ".";
}

}  // namespace tools
