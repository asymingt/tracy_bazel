# Phase 3: GUI dependency modules

Parent: [OVERVIEW](OVERVIEW.md). Can run in parallel with [PHASE2](PHASE2.md).

## 1. Objective

Get every library the GUI needs into the local registry. Each one is a
stand-alone, upstreamable module with its own smoke test, so Phase 4 is only
about Tracy itself.

## 2. Modules

### 2.1 `imgui` `1.93.0-docking` (BCR version bump)

- Base the overlay on the existing BCR `imgui` 1.92.6-docking.bcr.3 overlay
  (same target names, same backend split) so the change can go to the BCR as
  a simple version bump.
- The module stays **pristine upstream**. On top of its usual targets it
  exports `filegroup`s of its core sources, `misc/freetype` and the backends
  `glfw`/`opengl3`. Check whether the BCR overlay already does this.

**Where Tracy's 6 patches go.** A registry module cannot patch another
module's files, so here is the recommended split:

| Patch | Nature | Handling |
| --- | --- | --- |
| `imgui-no-default-font` | `imconfig.h` `#define` | `defines` on Tracy's private imgui library |
| `imgui-macos-clipboard` | `imconfig.h` `#define` | `defines` (macOS `select`) on the same library |
| `imgui-phantom-column` | core bug fix in `imgui_tables.cpp` (upstream #9519) | Check whether it is in a later imgui release. If not, ship a patched copy of `imgui_tables.cpp` in the tracy overlay |
| `imgui-no-samplers` | `imgui_impl_opengl3.cpp` behaviour change | patched copy in the tracy overlay |
| `imgui-loader` | `imgui_impl_opengl3_loader.h` additions | patched copy in the tracy overlay |
| `imgui-emscripten` | Emscripten only | dropped (out of scope) |

So `@tracy` defines a **private** `:imgui` `cc_library`. It compiles
`@imgui`'s unpatched source filegroups plus the 3 patched files from
`overlay/third_party/imgui/` (with the MIT license text). Its defines are
`IMGUI_ENABLE_FREETYPE`, `IMGUI_USE_WCHAR32` (public),
`IMGUI_DISABLE_DEFAULT_FONT`, `IMGUI_DISABLE_DEBUG_TOOLS` and
`IMGUI_DISABLE_DEMO_WINDOWS`. Only the profiler binary links it, so there
is no ODR clash with consumers who use `@imgui//:imgui`. A `tools/` script
regenerates the patched copies from upstream + `cmake/imgui-*.patch`.

> **Decision needed before Phase 3 starts.** If the 3 patched copies are
> unacceptable, the alternative is a hermetic patch applier (a small
> exec-config C++ tool that applies unified diffs inside a genrule). It's
> cleaner in principle, but it is more machinery to maintain.

### 2.2 `glfw` `3.5.1` (conditional)

First check whether `profiler/src/BackendGlfw.cpp` and imgui 1.93's
`imgui_impl_glfw.cpp` build against BCR `glfw` 3.4.0.bcr.4. Only if they
don't, add 3.5.1 to the registry as a BCR version bump, keeping the existing
overlay's Cocoa / Win32 / X11 split.

### 2.3 `nativefiledialog-extended` `1.4.1`

- Backends: Cocoa (macOS, `AppKit` + `UniformTypeIdentifiers`); Win32 COM
  (`ole32`, `uuid`, `shell32`); Linux xdg-desktop-portal over `@dbus`. That
  is the default and what Tracy uses (`NFD_PORTAL=ON`). The GTK backend is
  omitted because GTK isn't in the BCR.
- The Wayland parent-window support (`NFD_WAYLAND`) runs `wayland-scanner`
  on the bundled xdg-foreign XML.
- Tracy's `nfd-xdg-foreign-v2.patch` moves that from v1 to v2. Check nfd's
  later releases; otherwise carry the patch as a module patch with an
  upstream PR link.

### 2.4 `md4c` (commit `60e8065`)

C library (`md4c`, `md4c-html`). Version string follows the BCR convention
for unreleased commits (`<last tag>.bcr...` or a date-commit suffix);
settle it when packaging.

### 2.5 `base64` `0.5.2` (aklomp)

- C library whose SIMD codecs each need their own ISA flags (`-mssse3`,
  `-msse4.1`, `-msse4.2`, `-mavx`, `-mavx2`, `-mavx512vl/vbmi` on x86;
  NEON on arm64).
- CMake generates `config.h` (`HAVE_*` per codec).
- Overlay: one `cc_library` per codec with `select()`-ed copts by target
  CPU, and a static `config.h` per CPU, selected the same way. Runtime
  dispatch (upstream `codec_choose.c`) stays in place, so a single binary
  is correct on any CPU.

### 2.6 `tidy-html5` `5.8.0`

C library. Tracy's `tidy-cmake.patch` only touches CMake, so it isn't needed.
The overlay hard-codes what CMake would generate (version header, `SUPPORT_*`
defines).

### 2.7 `usearch` `2.26.4`

Header-only C++ (`include/usearch/*.hpp`) with `USEARCH_USE_SIMSIMD=0` and
`USEARCH_USE_FP16LIB=0` (as Tracy's build defaults). The SimSIMD and fp16
integrations are deferred.

### 2.8 `wayland-protocols` (latest release that has every protocol Tracy needs)

Data-only module: `exports_files` / `filegroup`s of the `stable/`, `staging/`
and `unstable/` XMLs. Tracy needs xdg-shell, xdg-activation-v1,
xdg-decoration-unstable-v1, fractional-scale-v1, viewporter,
cursor-shape-v1, tablet-unstable-v2 and xdg-toplevel-icon-v1. Choose the
version that includes xdg-toplevel-icon; nfd needs xdg-foreign-unstable-v2.
Also check whether the BCR `wayland` module already provides a
`wayland_scanner` rule. If it doesn't, write a small `wayland_protocol()`
macro and keep it in this module.

### 2.9 `dbus` `1.16.2`, `libglvnd` `1.7.0`

Copied byte for byte from `qt_bazel_workspace/modules/`. Any change is made
in both registries at the same time.

## 3. Tests

One `tests/<module>/` per module, linked by a consumer in the root
workspace, and each one also the `presubmit.yml` target:

- imgui: render one frame into a null backend.
- nfd: link test plus `NFD_Init`/`NFD_Quit`, skipped when there is no session
  bus.
- md4c: Markdown → HTML.
- base64: round-trip with each codec forced.
- tidy: clean a known HTML snippet.
- usearch: index and query 1k vectors.
- wayland-protocols: generate and compile all 8 Tracy protocols.

## 4. Exit criteria

- Every module builds across the full matrix (Linux-only modules are marked
  incompatible elsewhere with `target_compatible_with`), and its smoke test
  passes on the matching native hosts.
- `source.json` hashes, licenses and `metadata.json` all verified.

## 5. Risks

- base64's per-codec ISA flags under cross-compilation, especially Windows
  arm64 MinGW (NEON intrinsics headers).
- An upstream bump of imgui, glfw or freetype in the BCR while we work.
  Re-check against BCR head before filing PRs (Phase 5).

## 6. As built (2026-11-17)

| Module | Version | Notes |
| --- | --- | --- |
| `imgui` | `1.93.0-docking.tracy.1` | See below. |
| `nativefiledialog-extended` | `1.4.1` | Win32 COM, Cocoa, and Linux xdg-desktop-portal backends (no GTK). Linux has X11 and Wayland parenting. The Cocoa backend is compiled without ARC (`non_arc_srcs`). Carries Tracy's xdg-foreign v2 patch. The tarball's `3ps/wayland-protocols` submodule is empty, so the XML comes from `@wayland-protocols`. |
| `md4c` | `0.6.0.20261013` | Tracy's pinned commit, 38 commits after release 0.6.0. The fourth component is the date, so it sorts after 0.6.0 (a `-` suffix would make it a pre-release that sorts *before* 0.6.0). |
| `base64` | `0.5.2` | See below. |
| `tidy-html5` | `5.8.0` | Library only, all localizations. Tracy's `tidy-cmake.patch` touches only CMake and is not needed. |
| `usearch` | `2.26.4` | Header-only. No OpenMP or NumKong; software f16/bf16 on every compiler. Patch: `<Windows.h>` → `<windows.h>`, needed for cross-compiling from case-sensitive hosts. |
| `wayland-protocols` | `1.50` | Exports every XML file, plus `wayland_protocol.bzl`'s `wayland_client_protocol()`. That macro runs `wayland-scanner` directly with input and output paths as arguments (no shell), and has `basename`/`header` overrides. |
| `dbus` | `1.16.2` | Byte-identical copy from qt_bazel_workspace. |
| `libglvnd` | `1.7.0` | Byte-identical copy from qt_bazel_workspace. |

**imgui.** The module carries all six of Tracy's `cmake/imgui-*.patch` files.
It is tracy-flavoured rather than a pristine BCR bump (the user's choice),
hence the `.tracy.1` suffix, which keeps it from colliding with a future BCR
`1.93.0-docking`.
- It keeps the BCR target names (`//:imgui`,
  `//backends:platform-glfw`, `//backends:renderer-opengl3`) and both include
  spellings: `<imgui/imgui_impl_glfw.h>`, and Tracy's
  `<backends/imgui_impl_glfw.h>` relative to the source root.
- It deliberately does **not** define `IMGUI_DEFINE_MATH_OPERATORS`, because
  Tracy's `main.cpp` defines it and `TracyImGui.hpp` tests its value.
- The OpenGL loader loads the GL library at run time, so nothing GL-related is
  linked.
- `-lgdi32 -luser32 -lshell32` go on the GLFW backend for Windows, because BCR
  glfw only passes MSVC-style `-DEFAULTLIB:` flags, which MinGW ignores. Worth
  fixing in BCR glfw.

**base64.** A static `config/config.h` picks the codecs per CPU from compiler
macros: all x86 SIMD codecs on x86-64, NEON64 on arm64. Each x86 codec is its
own `cc_library`, so only its `codec.c` gets that codec's ISA flags, with MSVC
equivalents via a `cpu + compiler` `config_setting`. The run-time dispatcher
(`codec_choose.c`) keeps one binary correct on any CPU. Forcing a codec the
CPU lacks is undefined behaviour upstream, so the test gates forced codecs on
`__builtin_cpu_supports`.

**glfw.** `BackendGlfw.cpp` and imgui 1.93's GLFW backend use only symbols
present in 3.4, so BCR `glfw` 3.4.0.bcr.4 is used and no 3.5.1 module was
needed.

**Root workspace additions.**
- `osx.frameworks(...)`: the Cocoa stack for GLFW, nfd and imgui. `PrintCore`
  must be listed, because the `osx` extension only extracts
  `usr/include/cups` (needed via Carbon → ApplicationServices → PrintCore)
  when it is.
- Pins `libexpat` 2.8.3 and `rules_cc_autoconf` 0.27.0, and adds the
  bison/m4 `.bazelrc` flags for Windows execution platforms. These are copied
  from qt_bazel_workspace for Windows-hosted cross builds, and have not yet
  been verified in this repo's CI.

**Tests.** `tests/{imgui,nfd,md4c,base64,tidy,usearch,wayland_protocols}`:
- imgui: compile-time checks of the patched config and loader; FreeType
  loader present; both backends link.
- nfd: `NFD_Init` succeeds, or fails cleanly without a D-Bus session bus.
- md4c: Markdown → HTML.
- base64: every codec this CPU supports round-trips.
- tidy: repairs malformed HTML.
- usearch: 1000/1000 self-recall.
- wayland_protocols: all nine Tracy/nfd protocols generate, compile and link.

Verified locally on a linux-amd64 host:
- all 20 tests pass natively;
- `--config=tracy` builds for all five other targets;
- every new module also builds from an external workspace with the host's
  GCC 15 toolchain.
