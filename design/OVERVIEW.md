# Design: Tracy 0.15.0 for Bazel (`@tracy`)

Status: draft, 2026-11-16. Phase documents: [PHASE1](PHASE1.md) ·
[PHASE2](PHASE2.md) · [PHASE3](PHASE3.md) · [PHASE4](PHASE4.md) ·
[PHASE5](PHASE5.md).

## 1. Goal

Build the [Tracy](https://github.com/wolfpld/tracy) real-time profiler,
release `v0.15.0`, from source with Bazel, published as the module
**`tracy` at version `0.15.0`**. Like
[`qt_bazel_workspace`](../../qt_bazel_workspace), this repository is a
**staging registry** that will be upstreamed to the Bazel Central Registry
(BCR). It holds:

- a local registry (`bazel_registry.json` + `modules/<name>/<version>/`)
  with `tracy` and every dependency that is not yet in the BCR;
- smoke and integration tests under `tests/`, which exercise each module
  the way a consumer would;
- CI that builds every module natively and cross-compiles it across the full
  host × target matrix, and publishes the registry to GitHub Pages.

Upstream source: `https://github.com/wolfpld/tracy/archive/refs/tags/v0.15.0.tar.gz`,
`strip_prefix = "tracy-0.15.0"`,
sha256 `3d57da54aa67d7875da1e8fecd0bd5957675cef73c27dd52bffb6ab8d290a5e6`.

## 2. Decisions already taken

| Topic | Decision |
| --- | --- |
| Scope | Ship all three layers of Tracy, in phases: the instrumentation **client** library, then the headless **CLI tools**, then the ImGui **GUI profiler**. All of them live in the single `@tracy` module. |
| Toolchain / matrix | Same as the Qt workspace: the hermetic `@llvm` toolchain (Clang + libc++; MinGW sysroot on Windows); {linux, macos, windows} × {amd64, arm64}; 6 native jobs with tests plus 30 cross-compile jobs; Bazel 9.x (blocking) and `last_rc` (non-blocking). |
| Non-BCR dependencies | Each one becomes its own module in `modules/`, laid out so it can be upstreamed to the BCR on its own. Nothing is vendored into the `tracy` overlay unless §6 says so. |
| Linux GUI backend | Both upstream backends are supported and chosen by a flag: native Wayland is the default (as upstream), and GLFW/X11 (upstream's `LEGACY`) is the alternative. |
| LLM assistant | Included, so the GUI has the same features as upstream (curl, tidy-html5, usearch, base64). |
| Client configuration | Every `TRACY_*` option is a Bazel build setting under `@tracy//config`. The client is offered both as a static `cc_library` and as a `cc_shared_library`, so applications built from several shared libraries share one profiler instance. |
| ImGui | Add `imgui` `1.93.0-docking` to the local registry. It can go upstream as a BCR version bump. Tracy's own changes to imgui stay inside the `tracy` module; see Phase 3 §2.1. |
| Design docs | `design/*.md` in this repository (this file plus one file per phase), the same as the Qt workspace. |

## 3. Repository layout

The layout mirrors `qt_bazel_workspace`:

```
.bazelrc / .bazelversion (9.x) / .bazelignore / .gitattributes / .nojekyll
MODULE.bazel                 # root module "tracy_workspace"; @llvm + overrides
bazel_registry.json          # {"mirrors": []}
bazel_downloader.cfg         # mirrors for flaky hosts
platforms/BUILD.bazel        # //platforms:linux (glibc 2.34, host CPU)
patches/                     # root-only overrides (e.g. @llvm), never needed by modules
modules/
  tracy/{metadata.json, 0.15.0/{MODULE.bazel, source.json, presubmit.yml, overlay/, patches/}}
  capstone/ ppqsort/ imgui/ nativefiledialog-extended/ md4c/ base64/
  tidy-html5/ usearch/ wayland-protocols/ dbus/ libglvnd/ [glfw/]
tests/<phase-or-feature>/    # consumer-style smoke + integration tests
design/                      # this document set
.github/workflows/           # ci.yaml, pull_request.yaml, bazel.yaml, cache_cleanup.yaml
.github/scripts/render_build_status.py
```

Conventions inherited from the Qt workspace, which hold for every module:

- `<version>/MODULE.bazel` is byte-identical to `overlay/MODULE.bazel`.
- `source.json` lists the integrity hash of every overlay file and every patch.
- Every module declares a `@rules_license` `license()` target whose
  `license_text` points at the file in the upstream tarball.
- Every module has a `presubmit.yml` with `verify_targets` for BCR CI.
- Labels are always plain (`@zstd//:zstd`). Hardcoded canonical names
  (`@@x+//`) are not allowed (Qt REVIEW §1.1).
- A module must not depend on a root-only `single_version_override` patch
  (Qt REVIEW §1.2). If it needs a fix in a dependency, that fix is a new
  version of the dependency in the registry.
- Registries are listed in `.bazelrc` in this order: `file:///%workspace%/`
  first, then `https://bcr.bazel.build`.

## 4. Upstream build anatomy (what we are replacing)

Tracy builds with CMake (Meson is also offered) and fetches its dependencies
with CPM at configure time. Each of its own `cmake/*.cmake` files corresponds
to a Bazel target or rule:

| Upstream piece | Contents | Bazel equivalent |
| --- | --- | --- |
| `public/` (TracyClient) | Single-TU client (`TracyClient.cpp`), headers in `public/tracy/`, bundled libbacktrace | `@tracy//:client`, `:client_shared` (Phase 1) |
| `cmake/server.cmake` (TracyServer) | `public/common/*` + `server/*` | `@tracy//:server` (Phase 2) |
| `getopt/`, `dtl/`, `cmake/util.cmake` | in-tree helpers | private `cc_library`s (Phase 2) |
| `capture/ csvexport/ import/ update/ merge/` | `tracy-capture`, `tracy-capture-daemon`, `tracy-csvexport`, `tracy-import-chrome`, `tracy-import-fuchsia`, `tracy-update`, `tracy-merge` | `cc_binary` per tool (Phase 2) |
| `profiler/helpers/embed.cpp` + `Embed()` | compresses fonts, LLM prompts and the manual into C arrays | exec-config `cc_binary` + `tracy_embed` rule (Phase 4) |
| `cmake/manual.cmake` | `manual/tracy.tex` → pandoc 3.12.1 + Python → `tracy.md` → embedded | see Phase 4 §3 |
| `cmake/GitRef.cmake` | `git log` → `GitRef.hpp` | generated constant from the module version (Phase 2) |
| `cmake/config.cmake` | `-march=native`, LTO, mold, ccache | **dropped**: not hermetic, and wrong for cross-compiling |
| `profiler/` | GUI: ImGui + one backend (Wayland / GLFW / Emscripten) | `@tracy//:profiler` (Phase 4) |

## 5. Dependency inventory

Sources: `cmake/vendor.cmake` and `profiler/CMakeLists.txt`. "BCR now" is
the latest BCR version as of 2026-11-15.

| Dependency | Tracy pins | Needed by | BCR now | Plan |
| --- | --- | --- | --- | --- |
| zstd | v1.5.7 | server | 1.5.7.bcr.3 | BCR |
| nlohmann_json | v3.12.0 | import-chrome, GUI | 3.12.0.bcr.2 | BCR |
| capstone | 6.0.0-Alpha11 | server | — | **new module** (Phase 2) |
| PPQSort | 1.0.6 + 2 patches | server | — | **new module** (Phase 2) |
| freetype | VER-2-15-0 | GUI | 2.14.1.bcr.1 | BCR (check that 2.14 is enough) |
| imgui | v1.93.0-docking + 6 patches | GUI | 1.92.6-docking.bcr.3 | **new version** `1.93.0-docking` (Phase 3) |
| glfw | 3.5.1 | GUI (macOS, Windows, Linux-X11) | 3.4.0.bcr.4 | BCR if 3.4 is enough; otherwise a **new version** 3.5.1 (Phase 3) |
| nativefiledialog-extended | v1.4.1 + xdg-foreign-v2 patch | GUI | — | **new module** (Phase 3) |
| md4c | commit `60e8065` | GUI | — | **new module** (Phase 3) |
| base64 (aklomp) | v0.5.2 | GUI (LLM) | — | **new module** (Phase 3) |
| tidy-html5 | 5.8.0 (CMake-only patch) | GUI (LLM) | — | **new module** (Phase 3) |
| usearch | v2.26.4 | GUI (LLM) | — | **new module**, header-only (Phase 3) |
| pugixml | v1.16 | GUI (LLM) | 1.16 | BCR |
| curl | curl-8_23_0 (HTTP_ONLY) | GUI (LLM) | 8.23.0 | BCR |
| wayland (client, egl, cursor, scanner) | system | GUI Wayland, nfd | 1.24.0.bcr.1 | BCR (check the targets exist) |
| wayland-protocols | system | GUI Wayland, nfd | — | **new module**, data only (Phase 3) |
| xkbcommon | system | GUI Wayland | 1.9.2.bcr.beta.2 | BCR |
| EGL / GL | system | GUI Linux | — | `libglvnd` 1.7.0 copied from the Qt registry (link stubs) |
| dbus | system | nfd xdg-portal backend | — | `dbus` 1.16.2 copied from the Qt registry |
| libx11 | system | GUI Linux-X11 (via glfw) | 1.8.12.bcr.5 | BCR |

`dbus` and `libglvnd` already exist in the Qt registry, under the same
maintainer. They are copied byte for byte, so both workspaces upstream one
identical module and the two can never conflict.

## 6. Cross-cutting design

### 6.1 Root workspace

- `MODULE.bazel` starts from the Qt workspace's `@llvm` block: the same
  version (`0.8.25`), the same `single_version_override` patch set and the
  same `register_toolchains`. That way we inherit a matrix that is already
  proven, and we remove patches we don't need in Phase 5.
- `osx.frameworks(...)` is cut down to what Tracy and its dependencies link:
  roughly `AppKit`, `ApplicationServices`, `Cocoa`, `CoreFoundation`,
  `CoreGraphics`, `CoreVideo`, `IOKit`, `OpenGL`, `QuartzCore`,
  `UniformTypeIdentifiers`, `Foundation`, `Security` and `SystemConfiguration`
  (the last two for curl). The exact list is settled in Phase 4.
- `.bazelrc` keeps the toolchain, strict-env, platform-specific config,
  `ci` and sanitizer blocks. The Qt-only `per_file_copt` lines are replaced
  by one rule that silences warnings in third-party code:
  `.*external/(?!tracy)[^/]+/.*@-w`.

### 6.2 Client build settings (summary; details in Phase 1)

- `@tracy//config:<option>` is a `bool_flag` for each boolean `TRACY_*`
  option. Defaults match upstream, so **`enable` defaults to `False`**, as
  CMake's `TRACY_ENABLE=OFF` does.
- The flags become `defines` on `@tracy//:client`. They must propagate to
  consumers because `Tracy.hpp` macros expand differently depending on
  `TRACY_ENABLE` and the other options.

### 6.3 Hermeticity rules specific to Tracy

- No `-march=native`/`-mcpu=native`. That is upstream's `NO_ISA_EXTENSIONS=ON`.
- No `git` and no network access during the build. `GitRef.hpp` is generated
  from the module version.
- No `pkg-config`. Every library comes from a module.
- GUI runtime is the one exception: EGL/GL drivers and the
  compositor or X server are loaded from the host when the profiler runs.
  `libglvnd` only provides link-time stubs, as it does for Qt.

### 6.4 Testing strategy

| Level | What | Where |
| --- | --- | --- |
| Module smoke | One small consumer per new dependency module, which also forms its `presubmit.yml` target | `tests/<module>/` |
| Client | Compiles and runs with `enable` on and off; static and shared linking; two DSOs sharing one profiler | `tests/client/` |
| End-to-end | An instrumented workload is captured by `tracy-capture` over loopback, and `tracy-csvexport` output is checked for the expected zones | `tests/e2e/` (Phase 2) |
| GUI | Builds in all 36 matrix cells; on Linux, starts under a headless compositor and exits cleanly (CI only, tagged `manual` locally) | `tests/profiler/` (Phase 4) |

## 7. Phasing summary

| Phase | Theme | Exit criteria (abridged) |
| --- | --- | --- |
| [1](PHASE1.md) | Workspace scaffold + **client library** | Registry and CI cloned from the Qt workspace; `@tracy//:client` and `:client_shared` with all build settings; client tests green on 6 native and 30 cross jobs |
| [2](PHASE2.md) | **Server + CLI tools** | `capstone` and `ppqsort` modules; `@tracy//:server`; 7 CLI binaries; end-to-end capture test green on native jobs |
| [3](PHASE3.md) | **GUI dependency modules** | imgui 1.93.0-docking, nfd, md4c, base64, tidy-html5, usearch, wayland-protocols, dbus, libglvnd (+ glfw 3.5.1 if needed), each with a smoke test, green on the full matrix |
| [4](PHASE4.md) | **GUI profiler** | `@tracy//:profiler` with Wayland/X11 flag, LLM assistant, embedded fonts and manual; builds on all 36 cells; Linux headless run test |
| [5](PHASE5.md) | **Hardening + upstreaming** | REVIEW.md pass, sanitizer configs, `last_rc` green, pruned patches, README with status matrix, BCR PRs filed leaf-first |

Phases 1 and 2 are useful on their own: most users only want to instrument
their code (`@tracy//:client`) and capture traces in CI (`tracy-capture`).
Phase 3 can start in parallel with Phase 2, because the dependency modules
don't depend on each other's phase.

## 8. Open questions and risks

1. **Imgui patches across modules.** A registry module cannot patch another
   module. Phase 3 §2.1 recommends building a private imgui library inside
   `@tracy` from `@imgui` source filegroups, with patched copies of 3 files.
   That needs a decision before Phase 3.
2. **Manual generation.** The pandoc + Python step: check in the generated
   `tracy.md`, or fetch pinned pandoc binaries? Phase 4 §3 recommends checking
   it in.
3. **glfw 3.4 vs 3.5.1.** Does `BackendGlfw.cpp` or ImGui's GLFW backend at
   1.93 need 3.5 APIs? If so, we add `glfw` 3.5.1 to the local registry.
4. **capstone `6.0.0-Alpha11`.** A pre-release becomes a BCR module version;
   check BCR policy, and plan the bump when 6.0.0 is final.
5. **nfd `xdg-foreign-v2` patch** (203 lines). Check whether a later nfd
   release already has it; if not, the nfd module carries it as a patch.
6. **Windows on arm64 + MinGW.** The client and server rely on
   `TracyCpuid`, `dbghelp` and Winsock. The Qt workspace shows the toolchain
   works there, but Tracy itself has never been tested on that combination.
7. **Loopback inside the Bazel sandbox** for the end-to-end test. It should
   work on Linux and macOS; on Windows (no sandbox) watch for firewall prompts
   on runners.
8. **curl's TLS backend on macOS and Windows** in the BCR module (the LLM
   assistant may call HTTPS endpoints).
