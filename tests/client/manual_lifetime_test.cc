// TRACY_MANUAL_LIFETIME + TRACY_DELAYED_INIT: the profiler only exists between
// StartupProfiler() and ShutdownProfiler().

#include <cstdio>

#include <tracy/Tracy.hpp>

#if !defined(TRACY_MANUAL_LIFETIME) || !defined(TRACY_DELAYED_INIT)
#error "expected TRACY_MANUAL_LIFETIME and TRACY_DELAYED_INIT"
#endif

int main() {
  if (tracy::IsProfilerStarted()) {
    std::fprintf(stderr, "profiler started before StartupProfiler()\n");
    return 1;
  }
  tracy::StartupProfiler();
  {
    ZoneScopedN("managed");
    FrameMark;
  }
  const bool started = tracy::IsProfilerStarted();
  tracy::ShutdownProfiler();
  if (!started) {
    std::fprintf(stderr, "profiler not started after StartupProfiler()\n");
    return 1;
  }
  return 0;
}
