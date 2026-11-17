// End-to-end: this test is an instrumented program that profiles itself while
// tracy-capture records it over loopback. The trace is then checked with
// tracy-csvexport, round-tripped through tracy-update and tracy-merge, and a
// Chrome trace is imported with tracy-import-chrome.
//
// argv: capture csvexport update merge import-chrome (runfiles paths)

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include <tracy/Tracy.hpp>

#include "rules_cc/cc/runfiles/runfiles.h"
#include "tests/tools/process.h"

#if !defined(TRACY_ENABLE) || !defined(TRACY_MANUAL_LIFETIME)
#error "build with --@tracy//config:enable and manual_lifetime (see BUILD.bazel)"
#endif

using rules_cc::cc::runfiles::Runfiles;
using tools::Quote;
using tools::Run;

namespace {

bool ExpectZone(const std::string& csvexport, const std::string& trace, const char* zone) {
  std::string output;
  const int rc = Run(Quote(csvexport) + " " + Quote(trace), &output);
  if (rc != 0 || output.find(zone) == std::string::npos) {
    std::fprintf(stderr, "csvexport %s: exit %d, zone '%s' %s\n%s\n", trace.c_str(), rc, zone,
                 output.find(zone) == std::string::npos ? "missing" : "present", output.c_str());
    return false;
  }
  std::printf("csvexport %s: found '%s'\n", trace.c_str(), zone);
  return true;
}

void SetEnv(const char* name, const std::string& value) {
#ifdef _WIN32
  _putenv_s(name, value.c_str());
#else
  setenv(name, value.c_str(), 1);
#endif
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 6) {
    std::fprintf(stderr, "expected 5 tool paths\n");
    return 1;
  }
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest(BAZEL_CURRENT_REPOSITORY, &error));
  if (runfiles == nullptr) {
    std::fprintf(stderr, "runfiles: %s\n", error.c_str());
    return 1;
  }
  const std::string capture = runfiles->Rlocation(argv[1]);
  const std::string csvexport = runfiles->Rlocation(argv[2]);
  const std::string update = runfiles->Rlocation(argv[3]);
  const std::string merge = runfiles->Rlocation(argv[4]);
  const std::string import_chrome = runfiles->Rlocation(argv[5]);
  const std::string tmp = tools::TempDir();

  // A port per test run, so parallel runs do not collide. The profiler reads
  // TRACY_PORT when it starts, which manual lifetime lets us defer.
  const auto seed = static_cast<unsigned>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const std::string port = std::to_string(20000 + seed % 30000);
  SetEnv("TRACY_PORT", port);
  tracy::StartupProfiler();

  const std::string trace = tmp + "/capture.tracy";
  std::atomic<bool> done{false};
  int capture_rc = -1;
  std::string capture_output;
  std::thread capture_thread([&] {
    capture_rc = Run(Quote(capture) + " -o " + Quote(trace) + " -a 127.0.0.1 -p " + port +
                         " -s 2 -f",
                     &capture_output);
    done = true;
  });

  // Generate zones until the capture (2 s after connecting) has finished.
  for (int frame = 0; !done; ++frame) {
    {
      ZoneScopedN("e2e_zone");
      TracyPlot("e2e_frame", static_cast<int64_t>(frame));
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    FrameMark;
  }
  capture_thread.join();
  tracy::ShutdownProfiler();

  std::printf("tracy-capture exit %d\n%s\n", capture_rc, capture_output.c_str());
  if (capture_rc != 0) return 1;

  bool ok = ExpectZone(csvexport, trace, "e2e_zone");

  // Recompress with zstd and check the result is still readable.
  const std::string updated = tmp + "/updated.tracy";
  std::string output;
  if (Run(Quote(update) + " -z 3 " + Quote(trace) + " " + Quote(updated), &output) != 0) {
    std::fprintf(stderr, "tracy-update failed:\n%s\n", output.c_str());
    ok = false;
  } else {
    ok = ExpectZone(csvexport, updated, "e2e_zone") && ok;
  }

  // Merge the two captures.
  const std::string merged = tmp + "/merged.tracy";
  output.clear();
  if (Run(Quote(merge) + " -f -o " + Quote(merged) + " " + Quote(trace) + " " + Quote(updated),
          &output) != 0) {
    std::fprintf(stderr, "tracy-merge failed:\n%s\n", output.c_str());
    ok = false;
  } else {
    ok = ExpectZone(csvexport, merged, "e2e_zone") && ok;
  }

  // Import a minimal Chrome trace.
  const std::string json = tmp + "/chrome.json";
  {
    std::ofstream out(json);
    out << R"({"traceEvents":[)"
        << R"({"name":"chrome_zone","ph":"X","ts":0,"dur":250,"pid":1,"tid":1},)"
        << R"({"name":"chrome_zone","ph":"X","ts":500,"dur":100,"pid":1,"tid":1}]})";
  }
  const std::string imported = tmp + "/imported.tracy";
  output.clear();
  if (Run(Quote(import_chrome) + " " + Quote(json) + " " + Quote(imported), &output) != 0) {
    std::fprintf(stderr, "tracy-import-chrome failed:\n%s\n", output.c_str());
    ok = false;
  } else {
    ok = ExpectZone(csvexport, imported, "chrome_zone") && ok;
  }

  return ok ? 0 : 1;
}
