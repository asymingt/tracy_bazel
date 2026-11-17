# Phase 4: GUI profiler

Parent: [OVERVIEW](OVERVIEW.md). Depends on: [PHASE2](PHASE2.md),
[PHASE3](PHASE3.md).

## 1. Objective

Build `tracy-profiler`, upstream's ImGui GUI, from `@tracy//:profiler` on
all six target platforms. It has the same features as upstream (LLM
assistant included), and on Linux the window backend is selected by a flag.

## 2. Targets and settings

| Item | Details |
| --- | --- |
| `@tracy//:profiler` | `cc_binary` (alias `tracy-profiler`). Sources: `profiler/src/**` minus the other backends and the Emscripten files. Deps: `:server`, `:imgui` (private, Phase 3 §2.1), `@freetype`, `@nativefiledialog-extended`, `@md4c`, `@base64`, `@tidy-html5`, `@usearch`, `@pugixml`, `@curl`, `@nlohmann_json`, embedded data (§3) |
| `config:linux_backend` | `string_flag`, values `wayland` (default) and `x11`. Ignored on macOS and Windows, which always use GLFW |
| Wayland backend | `BackendWayland.cpp` plus 8 protocols generated with `wayland-scanner` (from `@wayland-protocols`); deps `@wayland//:wayland-client`/`-egl`/`-cursor`, `@xkbcommon`, `@libglvnd` (EGL + GL) |
| X11 backend | `BackendGlfw.cpp` + `imgui_impl_glfw.cpp`; `@glfw` (X11), `@libx11`, `@libglvnd` (GL) |
| macOS | `BackendGlfw.cpp`; frameworks `Cocoa`, `OpenGL`, `IOKit`, `CoreVideo`, `ApplicationServices` |
| Windows | `BackendGlfw.cpp`; `opengl32`, `gdi32`, `ws2_32`, `dbghelp`, `ole32`; the icon `.rc` and DPI manifest go in Phase 5 (they need `llvm-rc` wiring) |
| `config:no_fileselector` | `bool_flag` → `NO_FILESELECTOR`, which drops the nfd dependency |
| `config:self_profile` | `bool_flag` → links `:client` with `enable=True` into the profiler (upstream `SELF_PROFILE`) |

## 3. Generated sources

- **`embed` tool:** an exec-config `cc_binary` from
  `profiler/helpers/embed.cpp`, wrapped by a `tracy_embed(name, src, text)`
  rule that produces `data/<Name>.cpp/.hpp`. It is applied to the 8 fonts,
  the 5 LLM prompt/skill files plus `tools.json`, the achievements Markdown,
  and the manual. This replaces upstream's `ExternalProject_Add(embed)`.
- **Manual (`tracy.md`):** upstream generates it from `manual/tracy.tex`
  with pinned pandoc **3.12.1** plus `latex2md.py` and `filter.lua`.
  - **Recommended: check in** the generated `overlay/manual/tracy.md`, with a
    `tools/regen_manual.sh` that runs the upstream script using a local
    pandoc 3.12.1. The build then has no Python or pandoc in the action graph,
    and the manual only changes when the Tracy version does.
  - Alternative: download the pinned pandoc release binaries per exec
    platform with `http_archive`, plus `rules_python`. Pandoc publishes no
    Windows arm64 build, which would rely on x86-64 emulation (the same
    workaround the Qt workspace uses for Perl).
- **`GitRef.hpp`:** reuse Phase 2's `:git_ref`.

## 4. Tests

| Test | Checks |
| --- | --- |
| `build_test` | `@tracy//:profiler` in all 36 matrix cells, and on Linux once for each `linux_backend` value |
| `tests/profiler/embed_test` | Embedded blobs decompress to the original file bytes (font, prompt) |
| `tests/profiler/headless_run_test` (Linux native CI only, tagged `manual`) | Wayland: start `weston --backend=headless` and run the profiler for ~5 s, opening a `.tracy` from Phase 2's end-to-end test via the command line, then SIGTERM. X11: the same under `Xvfb`. Uses Mesa from the host (apt), as allowed by OVERVIEW §6.3 |

A runtime smoke test on macOS and Windows is optional, since hosted runners do
have a desktop session. Add it in Phase 5 if it turns out to be stable.

## 5. Exit criteria

- `bazel build @tracy//:profiler` is green on 6 native and 30 cross jobs.
- Linux headless run test green for both backends on linux-amd64 and
  linux-arm64.
- Manually opening a capture works on a real desktop on each OS
  (one-time sign-off, recorded in the PR).

## 6. Risks

- The profiler is upstream's biggest and fastest-changing component.
  Unity-build-style includes and platform `#ifdef`s may need per-file source
  selection. Expect several iterations per OS.
- Wayland protocol versions vs the BCR `wayland` 1.24 core (upstream warns
  that libwayland without core protocol v6 loses functionality).
- curl and OpenSSL add sizeable build time to every GUI cell. Watch the CI
  disk-cache budget; it was the binding constraint in the Qt workspace.

## 7. As built (2026-11-17)

- **Package layout.** The GUI lives in the `//profiler` package of the
  overlay (`@tracy//profiler:tracy-profiler`), with root aliases
  `@tracy//:tracy-profiler` and `@tracy//:profiler`. The source list is
  upstream's: `src/*.cpp`, `src/ini.c` and all of `src/profiler/*.cpp`
  (glob verified against the CMake list). The backend is chosen by
  `select()`.
- **Flags** (`@tracy//config`):
  - `linux_backend=wayland|x11`, which maps to upstream `LEGACY`;
  - `no_fileselector`, which maps to upstream `NO_FILESELECTOR`;
  - `self_profile`, which maps to upstream `SELF_PROFILE`.

  `self_profile` compiles `public/TracyClient.cpp` into the binary with
  `TRACY_ENABLE`, as upstream does.
- **Embedding.**
  - `profiler/embed.bzl` (`tracy_embed`) runs upstream's
    `helpers/embed.cpp`, built for the execution platform, with no shell. It
    produces `gen/data/<Name>.{cpp,hpp}` for the 8 fonts, 6 LLM files, 45
    achievement texts and the manual.
  - The achievement names are derived from the file names, matching upstream's
    `Text<Name>`.
  - The `Font Awesome 7 Free-Solid-900.otf` file name (with spaces) is fine
    as a Bazel 9 label.
- **Manual.** It is checked in at `overlay/profiler/generated/tracy.md`.
  `tools/regen_manual.sh` regenerates it with upstream's pinned pandoc 3.12.1
  (SHA-256 checked) and `manual/latex2md.py`, with `SOURCE_DATE_EPOCH` pinned
  to the v0.15.0 release date so the output is byte-reproducible (the manual's
  `\today` otherwise changes daily). The build needs no pandoc and no Python.
- **New tracy patches.**
  - `embed_include_basename.patch`: the generated `.cpp` includes its header by
    file name rather than by the full output path.
  - `libcxx_atomic_shared_ptr.patch`: libc++ (LLVM 21 and Apple's) does not
    implement `std::atomic<std::shared_ptr<T>>`, so this falls back to the
    C++11 free functions when `__cpp_lib_atomic_shared_ptr` is not defined.
  - `mingw_profiler.patch`: includes `<errno.h>` everywhere, and provides
    `memmem()` on `_WIN32`, not only `_MSC_VER`.
- **Root-only workarounds for the MinGW toolchain**, both candidates to send to
  the BCR:
  - `patches/curl/windows_mingw.patch`: `-D` instead of `/D`, plus `-l`
    libraries next to `-DEFAULTLIB:`.
  - `-lpthread` on Windows: under MinGW, BCR BoringSSL uses winpthreads
    (`crypto/internal.h`), which the `@llvm` MinGW runtime ships as
    `libpthread.a`.
- **macOS:** `FontServices` joins `osx.frameworks`. `ApplicationServices`'
  `ATS.framework` symlinks two `.tbd` files into
  `PrivateFrameworks/FontServices.framework`, and the darwin sandbox rejects
  the dangling links.
- **Issue #1 (Windows "stack smashing detected").**
  - The Windows bisect showed every option variant failing 25–55% of runs, so
    the cause is not a Tracy feature.
  - Root cause: mingw-w64 initialises `__stack_chk_guard` in a
    default-priority constructor, while Tracy's static profiler object starts
    worker threads during C++ static initialisation. Frames entered before the
    guard is randomised fail their check on return.
  - `bazel/mingw_stack_guard.c` (Windows only, compiled into `:client` and
    `:client_dynamic`) initialises the guard identically from a priority-101
    constructor, so the runtime's own initialiser then leaves it alone.
- **Tests** (`tests/profiler`):
  - `profiler_build_test` builds the GUI in every matrix cell.
  - `headless_{wayland,x11}_test` are `manual` and run by a dedicated Linux CI
    step that installs weston, Xvfb, Mesa and the X client libraries. Each
    starts the profiler under `weston --backend=headless` or Xvfb with
    `LIBGL_ALWAYS_SOFTWARE=1`, and requires it to still be running after 8 s.
  - Locally, the Wayland build ran for 6 s on a real GNOME/Wayland session,
    and the GLFW build ran as well. GLFW 3.4 picks its Wayland platform there
    whenever `WAYLAND_DISPLAY` is set, which is upstream GLFW behaviour.
