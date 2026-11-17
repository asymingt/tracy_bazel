// A shared library that is instrumented with Tracy. Built twice (PLUGIN_NAME
// = plugin_a / plugin_b); each reports the address of the profiler instance it
// sees, which must be the one owned by @tracy//:client_shared.

#include <tracy/Tracy.hpp>

#if defined(_WIN32)
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#define PLUGIN_CONCAT2(a, b) a##b
#define PLUGIN_CONCAT(a, b) PLUGIN_CONCAT2(a, b)

extern "C" PLUGIN_EXPORT const void* PLUGIN_CONCAT(PLUGIN_NAME, _profiler)() {
  ZoneScopedN("plugin");
#ifdef TRACY_ENABLE
  return &tracy::GetProfiler();
#else
  // Only reached when the plugin is built standalone by `bazel build //...`.
  return nullptr;
#endif
}
