// Links nativefiledialog-extended and exercises init/teardown without opening
// a dialog. On Linux the portal backend needs a D-Bus session bus, which test
// sandboxes usually lack, so NFD_Init() may fail there; it must fail cleanly.

#include <cstdio>

#include <nfd.h>

int main() {
  const nfdresult_t rc = NFD_Init();
  if (rc == NFD_OKAY) {
    std::printf("NFD_Init: ok\n");
    NFD_Quit();
    return 0;
  }
  const char* error = NFD_GetError();
  std::printf("NFD_Init: error (%s)\n", error ? error : "<null>");
#if defined(__linux__)
  // Acceptable without a session bus, as long as an error is reported.
  return error != nullptr ? 0 : 1;
#else
  return 1;
#endif
}
