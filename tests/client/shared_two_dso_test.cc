// Two instrumented shared libraries plus the executable must all see the same
// profiler instance when they link @tracy//:client_shared.

#include <cstdio>

#include <tracy/Tracy.hpp>

#if defined(_WIN32)
#define PLUGIN_IMPORT __declspec(dllimport)
#else
#define PLUGIN_IMPORT
#endif

extern "C" PLUGIN_IMPORT const void* plugin_a_profiler();
extern "C" PLUGIN_IMPORT const void* plugin_b_profiler();

int main() {
  ZoneScopedN("main");
  const void* self = &tracy::GetProfiler();
  const void* a = plugin_a_profiler();
  const void* b = plugin_b_profiler();
  std::printf("main=%p plugin_a=%p plugin_b=%p\n", self, a, b);
  if (self != a || self != b) {
    std::fprintf(stderr, "more than one profiler instance in the process\n");
    return 1;
  }
  return 0;
}
