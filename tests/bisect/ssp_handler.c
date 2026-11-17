/* Diagnostic only (issue #1): replaces mingw-w64's __stack_chk_fail with one
 * that prints the failing thread and a raw backtrace (module-relative
 * offsets), so the frame can be symbolised offline with llvm-symbolizer. */
#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((noreturn, no_stack_protector)) void __stack_chk_fail(void) {
  void* frames[48];
  USHORT n = CaptureStackBackTrace(0, 48, frames, NULL);
  HMODULE exe = GetModuleHandleA(NULL);
  fprintf(stderr, "SSP-FAIL thread=%lu frames=%u exe_base=%p\n", GetCurrentThreadId(), n, (void*)exe);
  for (USHORT i = 0; i < n; ++i) {
    HMODULE mod = NULL;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)frames[i], &mod)) {
      GetModuleFileNameA(mod, name, sizeof(name));
    }
    fprintf(stderr, "SSP-FRAME %u %s +0x%llx\n", i, name,
            (unsigned long long)((char*)frames[i] - (char*)mod));
  }
  fflush(stderr);
  TerminateProcess(GetCurrentProcess(), 3);
  for (;;) {}
}
#endif
