// Default configuration: TRACY_ENABLE is off, so every macro must compile to
// nothing and the profiler must never start.

#include <cstdio>

#include <tracy/Tracy.hpp>

#ifdef TRACY_ENABLE
#error "TRACY_ENABLE must not be defined without --@tracy//config:enable"
#endif

namespace {

int Work(int n) {
  ZoneScoped;
  ZoneText("work", 4);
  int sum = 0;
  for (int i = 0; i < n; ++i) {
    ZoneScopedN("inner");
    sum += i;
  }
  TracyPlot("sum", static_cast<int64_t>(sum));
  TracyMessageL("done");
  return sum;
}

}  // namespace

int main() {
  const int sum = Work(10);
  FrameMark;
  if (sum != 45) {
    std::fprintf(stderr, "unexpected sum %d\n", sum);
    return 1;
  }
  if (TracyIsStarted || TracyIsConnected) {
    std::fprintf(stderr, "profiler reports running while disabled\n");
    return 1;
  }
  return 0;
}
