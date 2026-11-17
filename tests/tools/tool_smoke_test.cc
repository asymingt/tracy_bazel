// Every tool must start, print its banner with the module version, and exit
// with an error when given no arguments. Catches link and loader failures on
// each native host.

#include <cstdio>
#include <memory>
#include <string>

#include "rules_cc/cc/runfiles/runfiles.h"
#include "tests/tools/process.h"

using rules_cc::cc::runfiles::Runfiles;

int main(int argc, char** argv) {
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest(BAZEL_CURRENT_REPOSITORY, &error));
  if (runfiles == nullptr) {
    std::fprintf(stderr, "runfiles: %s\n", error.c_str());
    return 1;
  }
  int failures = 0;
  for (int i = 1; i < argc; ++i) {
    const std::string path = runfiles->Rlocation(argv[i]);
    std::string output;
    const int rc = tools::Run(tools::Quote(path), &output);
    const bool ok = rc != 0 && output.find("0.15.0") != std::string::npos;
    std::printf("%s %s (exit %d)\n", ok ? "ok  " : "FAIL", argv[i], rc);
    if (!ok) {
      std::printf("%s\n", output.c_str());
      ++failures;
    }
  }
  return failures == 0 ? 0 : 1;
}
