# Pre-upstreaming review of `modules/`

Date: 2026-11-17. Scope: the 12 modules under `modules/` (`tracy` plus 11
dependencies), the root workspace configuration, and BCR contribution rules.
Follows the checklist from qt_bazel_workspace's `design/REVIEW.md`.

## Checked mechanically (all pass)

- `tools/sync_overlay.py --all` is a no-op. Every overlay and patch hash in
  `source.json` matches the files on disk, and every `<version>/MODULE.bazel`
  is byte-identical to `overlay/MODULE.bazel`.
- No canonical repository names (`@@x+//`) in any overlay `.bazel`/`.bzl`.
- Every `@repo//` label used by a module is one of its own `bazel_dep`s.
  Labels inside macros are wrapped in `Label()`, so they resolve in the
  defining module (`wayland_protocol.bzl`), not in the caller.
- Every `license_text` path exists in the upstream archive. Every module has a
  `license()` target with SPDX kinds:
  - `tracy`: BSD-3-Clause;
  - `capstone`: BSD-3-Clause + NCSA;
  - `ppqsort`: MIT;
  - `imgui`: MIT;
  - `nativefiledialog-extended`: Zlib;
  - `md4c`: MIT;
  - `base64`: BSD-2-Clause;
  - `tidy-html5`: HTMLTIDY;
  - `usearch`: Apache-2.0;
  - `wayland-protocols`: MIT;
  - `dbus` and `libglvnd`: as in qt_bazel_workspace.
- Every `metadata.json` has homepage, maintainers, repository and versions,
  and lists the version directory.
- Every module has a `presubmit.yml`.

## Built outside this workspace

`.github/workflows/consumer.yaml` builds a fresh consumer workspace on Ubuntu
(GCC), macOS (Apple clang) and Windows (MSVC):
- it uses only this checkout as a registry, ahead of the BCR;
- it uses the host's default toolchain and none of this repo's `.bazelrc`
  flags or root overrides;
- it builds the client (static, shared, enabled), all CLI tools, and the GUI.

That is the closest available approximation of BCR presubmit. Locally, the
same was verified with GCC 15 + libstdc++ for the client, the tools and every
dependency module.

## Findings

### F1. Workarounds that only exist in this workspace's root (MinGW + `@llvm`)

None of these is needed by the modules under MSVC, GCC or Apple clang. They
are needed for the hermetic MinGW toolchain this workspace tests with, and
should go to the BCR as fixes to the respective modules:

| Root item | Module it works around | Upstream fix |
| --- | --- | --- |
| `patches/curl/windows_mingw.patch` | BCR `curl` uses `/D` defines and `-DEFAULTLIB:` only | Use `-D` (cl.exe accepts it) and add `-l` libs |
| `-lgdi32 -luser32 -lshell32` in `imgui//backends:platform-glfw` | BCR `glfw` links Win32 libs with `-DEFAULTLIB:` only | Same, in glfw |
| `-lpthread` in `tracy//profiler` (Windows) | BCR `boringssl` uses pthreads under MinGW but never links them | `linkopts = ["-lpthread"]` for MinGW in boringssl |
| `patches/llvm/windows_pkg_dir_symlinks.patch` | `@llvm`'s macOS SDK extraction on a Windows host leaves framework directory symlinks untraversable | Shared with qt_bazel_workspace; to send to `@llvm` |
| `.bazelrc` `--nointerface_shared_objects` (Windows host) | rules_cc does not use a link `@params` file for non-Windows shared libraries that emit an interface library | Report to rules_cc (or have `@llvm` only emit `.ifso` where params files are not needed) |
| `.bazelrc` bison/m4 `per_file_copt` | `rules_bison`/`rules_m4` gnulib config for MinGW exec | (from qt_bazel_workspace; in their queue) |

### F2. Patches carried, with upstream status

| Module | Patch | Upstream |
| --- | --- | --- |
| `tracy` | `mingw_arm64_cntvct.patch` | to send (MinGW arm64 timer) |
| `tracy` | `embed_include_basename.patch` | to send (build-system-agnostic embed) |
| `tracy` | `libcxx_atomic_shared_ptr.patch` | to send (libc++ lacks P0718) |
| `tracy` | `mingw_profiler.patch` | to send (`errno.h`, `memmem`) |
| `tracy` | `windows_safe_file_write.patch` | to send (rename of an open file fails on Windows; affects MSVC too) |
| `ppqsort` | `counting_semaphore.patch` | Tracy's fix; to send to GabTux/PPQSort |
| `ppqsort` | `no_execution_header.patch` | to send (unused include pulls in TBB) |
| `nativefiledialog-extended` | `xdg_foreign_v2.patch` | Tracy's; to send to btzy/nativefiledialog-extended |
| `usearch` | `lowercase_windows_h.patch` | to send |
| `imgui` | Tracy's six `imgui-*.patch` | phantom-column = ocornut/imgui#9519 (open); the rest are Tracy configuration |

### F3. `imgui` 1.93.0-docking.tracy.1 is Tracy-specific

By design (decided in Phase 3), it carries Tracy's configuration patches
(`IMGUI_DISABLE_DEFAULT_FONT`, the macOS clipboard, the GL loader additions),
which change behaviour for every user of that version. It should not go to the
BCR under the `imgui` name as-is. Options for upstreaming:
1. Rename it to a Tracy-private module (e.g. `tracy_imgui`).
2. Submit a pristine `imgui` 1.93.0-docking to the BCR, and move Tracy's
   patched files into the `tracy` overlay (the Phase 3 design's original
   recommendation).

Both require a maintainer decision before BCR submission.

### F4. Windows DLL naming with MinGW toolchains

`cc_shared_library` names outputs `lib<name>.so` under the `@llvm` MinGW
toolchain, which `-l<name>` cannot find. `@tracy//:client_shared` sets
`shared_lib_name` explicitly, and consumers must do the same for their own
DLLs. This is documented in the README, and is really a toolchain/rules_cc
issue.

### F5. Issue #1: MinGW stack-protector guard initialisation order

Fixed in the module (`bazel/mingw_stack_guard.c`, `alwayslink`). It was
diagnosed from a symbolised CI backtrace: the abort happens in
`tracy::__wsinit` (`WSAStartup`), reached from the worker thread's
`GetHostInfo()`. Verified 40/40 on windows-amd64 and windows-arm64, against
31/40 failures before the fix. The root cause is in
mingw-w64: the guard is initialised from a default-priority constructor. It is
worth reporting there and to the `@llvm` toolchain, since any program that
starts threads from a static initialiser is affected.

### F6. Sanitizers (Linux, `--config=asan|ubsan|tsan`)

Run over `tests/{client,tools,ppqsort,base64}`:

- **ASan:** clean, except `shared_two_dso_test`. The toolchain links libc++
  statically into `libTracyClient.so` while the executable uses
  `libc++.so.1`, and ASan reports the duplicated libc++ globals as an ODR
  violation. That is a linking artefact of the sanitizer configuration, so the
  test is tagged `noasan`.
- **UBSan:** clean, except `e2e_test`, which exercises Tracy's wire protocol
  and symbol resolution. Both findings are upstream:
  - `tracy::MemRead<T>()` (`public/common/TracyAlign.hpp`) reads unaligned
    data through a misaligned `const T*` before `memcpy`ing it.
  - libbacktrace's `dwarf.cpp` stores vendor tags such as `DW_TAG` 0x4107 in
    `enum dwarf_tag`.

  The test is tagged `noubsan`; both are worth reporting upstream (the first
  is fixed by taking `const void*`).
- **TSan:** clean, except in upstream code that two tests drive hard (both
  tagged `notsan`):
  - `ppqsort_test`: races in `swap_blocks`
    (`parallel/cpp/partition_par.h`), which synchronises through its own
    atomic flags and `atomic_ref`.
  - `e2e_test`: races in Tracy's lock-free client internals, namely the
    thread-name list (`GetThreadNameData` vs `SetThreadNameWithHint`), the
    bundled rpmalloc's deferred frees, and `Profiler::SendString`.

  Worth reporting upstream; neither is introduced by the packaging. All other
  client tests (static, shared, fibers, on-demand, manual lifetime) are
  TSan-clean.

## Deferred (explicitly out of scope)

- Windows icon and DPI manifest for `tracy-profiler.exe`. Upstream compiles
  `win32/Tracy.rc`/`Tracy.manifest` with the resource compiler, and `rules_cc`
  has no portable resource-compiler action (MSVC `rc.exe` vs `llvm-windres`).
  The binary works without them; it uses the default icon and is not
  DPI-aware.
- macOS `.app` bundle, Emscripten/WASM GUI, Python and Fortran bindings,
  libunwind/debuginfod/ROCprof client options, `-march`-tuned server builds.
- **Filing the BCR pull requests themselves.** That is an outward-facing action
  to take with the maintainer's explicit go-ahead, once F3 is decided. The
  modules are laid out for a direct copy into a BCR checkout, leaf-first:
  1. `capstone`, `ppqsort`, `md4c`, `base64`, `tidy-html5`, `usearch`,
     `wayland-protocols`;
  2. `dbus`, `libglvnd`;
  3. `nativefiledialog-extended`, `imgui` (per F3);
  4. `tracy`.
