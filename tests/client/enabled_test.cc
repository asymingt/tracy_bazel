// TRACY_ENABLE on: the profiler starts, accepts events without a connected
// server, and the process exits normally (TRACY_NO_EXIT is off).

#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

#include <tracy/Tracy.hpp>

#ifndef TRACY_ENABLE
#error "TRACY_ENABLE must be defined by @tracy//config"
#endif

extern "C" int c_api_smoke(void);

namespace {

TracyLockable(std::mutex, g_lock);
int g_counter = 0;

void Worker(int id) {
  tracy::SetThreadName(id == 0 ? "worker-0" : "worker-1");
  for (int i = 0; i < 100; ++i) {
    ZoneScopedN("worker iteration");
    ZoneValue(i);
    std::lock_guard<LockableBase(std::mutex)> guard(g_lock);
    ++g_counter;
  }
}

}  // namespace

int main() {
  TracySetProgramName("enabled_test");
  {
    ZoneScopedNC("main", 0x00ff00);
    std::vector<std::thread> threads;
    for (int i = 0; i < 2; ++i) threads.emplace_back(Worker, i);
    for (auto& t : threads) t.join();
    TracyPlot("counter", static_cast<int64_t>(g_counter));
    TracyMessageL("workers joined");
    int* p = new int(7);
    TracyAlloc(p, sizeof(int));
    TracyFree(p);
    delete p;
    FrameMark;
  }

  if (g_counter != 200) {
    std::fprintf(stderr, "unexpected counter %d\n", g_counter);
    return 1;
  }
  if (!TracyIsStarted) {
    std::fprintf(stderr, "profiler did not start\n");
    return 1;
  }
  if (TracyIsConnected) {
    std::fprintf(stderr, "no server should be connected\n");
    return 1;
  }
  return c_api_smoke();
}
