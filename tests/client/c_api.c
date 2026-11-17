/* C API smoke test, compiled as C. */

#include <tracy/TracyC.h>

int c_api_smoke(void) {
  TracyCZoneN(ctx, "c zone", 1);
  TracyCZoneValue(ctx, 42);
  TracyCPlot("c plot", 1.5);
  TracyCMessageL("from C");
  TracyCFrameMark;
  TracyCZoneEnd(ctx);
#ifdef TRACY_ENABLE
  /* No server is running, so the client must report "not connected". */
  return TracyCIsConnected ? 1 : 0;
#else
  return 0;
#endif
}
