# Phase 1: Workspace scaffold and client library

Parent: [OVERVIEW](OVERVIEW.md).

## 1. Objective

Stand up the registry, the root workspace and the CI by cloning
`qt_bazel_workspace`, and ship `@tracy//:client`. That is all a typical user
needs to instrument a Bazel-built program.

## 2. Deliverables

### 2.1 Workspace scaffold

- Copy from the Qt workspace, then adapt:
  - `.bazelrc`, `.bazelversion`, `.bazelignore`, `.gitattributes`, `.nojekyll`
  - `bazel_registry.json`, `bazel_downloader.cfg` (emptied)
  - `platforms/BUILD.bazel`
  - `patches/llvm/*`
  - `.github/workflows/*`, `.github/scripts/render_build_status.py`
- Root `MODULE.bazel` (`module(name = "tracy_workspace")`): the `@llvm`
  block from OVERVIEW §6.1, plus `bazel_dep(name = "tracy", version = "0.15.0")`
  and the BCR dependencies the tests need.
- `.bazelrc`: drop the Qt-specific lines (`QT_FORCE_STDERR_LOGGING`,
  `config:qt`, the Qt `per_file_copt` regexes); add the third-party `-w` rule.
- The CI workflows stay unchanged except for the repo name, the status
  branch and the badge URLs. The target pattern is `//... @tracy//...`.

### 2.2 `modules/tracy/0.15.0/`

- `source.json`: URL, `strip_prefix = "tracy-0.15.0"`, integrity hash, and the
  hashes of the overlay files.
- `overlay/MODULE.bazel`: `module(name = "tracy", version = "0.15.0",
  compatibility_level = 0)` with dependencies on `platforms`, `rules_cc`,
  `rules_license` and `bazel_skylib` (for the flags).
- `overlay/BUILD.bazel`: `license(name = "license", license_kinds =
  BSD-3-Clause, license_text = "LICENSE")`, plus the client targets below.
- `overlay/config/BUILD.bazel` and `overlay/config/flags.bzl`: build settings.
- `metadata.json`: copy the maintainer block from the Qt modules;
  `repository = ["github:wolfpld/tracy"]`.
- `presubmit.yml`: `verify_targets` on `@tracy//:client`, run on
  `ubuntu2204` and `macos_arm64`, with the same flags the Qt modules pass.

### 2.3 Client targets

| Target | Kind | Notes |
| --- | --- | --- |
| `:client` | `cc_library` | `srcs = ["public/TracyClient.cpp"]` (upstream's unity TU). Everything else under `public/client`, `public/common` and `public/libbacktrace` goes in `textual_hdrs`/`hdrs`. `includes = ["public"]`, so consumers write `#include <tracy/Tracy.hpp>`. `defines` come from config (§3). |
| `:client_dynamic` | `cc_library` | Same sources, with `local_defines = ["TRACY_EXPORTS"]` and `defines = ["TRACY_IMPORTS"]`. Consumers depend on this together with `dynamic_deps = ["@tracy//:client_shared"]`. |
| `:client_shared` | `cc_shared_library` | `deps = [":client_dynamic"]`, so there is one profiler instance per process. |
| `:headers` | `cc_library` | Headers only (`TracyVulkan.hpp`, `TracyOpenGL.hpp`, `TracyD3D12.hpp`, `TracyCUDA.hpp`, `TracyLua.hpp`, `TracyC.h`, ...) so a library can be instrumented without linking the client. The binary still has to link `:client` or `:client_shared`. |

Per-platform `linkopts`, matching upstream's `CMakeLists.txt`: `-lpthread -ldl`
on Linux; `ws2_32`, `dbghelp` and `advapi32` on Windows (MinGW); nothing extra
on macOS.

## 3. Build settings

`flags.bzl` defines a macro `tracy_bool_flag(name, define)` that emits a
`bool_flag`, a `config_setting` for the value `True`, and a `select()`
fragment. `config/BUILD.bazel` instantiates one flag per upstream option:

| Flag (`--@tracy//config:...`) | Define | Default |
| --- | --- | --- |
| `enable` | `TRACY_ENABLE` | False (as upstream) |
| `on_demand` | `TRACY_ON_DEMAND` | False |
| `no_callstack`, `no_callstack_inlines`, `no_sampling`, `no_system_tracing`, `no_context_switch`, `no_wait_stacks`, `no_vsync_capture`, `no_frame_image`, `no_code_transfer`, `no_broadcast`, `no_exit`, `no_verify`, `no_crash_handler`, `no_internal_message` | `TRACY_NO_*` | False |
| `only_localhost`, `only_ipv4`, `fibers`, `manual_lifetime`, `delayed_init`, `timer_fallback`, `disallow_hw_timer`, `symbol_offline_resolve`, `libbacktrace_elf_dynload_support`, `ignore_memory_faults`, `opengl_auto_calibration`, `patchable_nopsleds`, `verbose`, `demangle` | as upstream | False |
| `callstack` | `TRACY_CALLSTACK=<n>` | `int_flag`, unset |
| `platform_header` | `TRACY_PLATFORM_HEADER="..."` | `label_flag`; the header is added to `:client`'s deps when set |

These options need dependencies we don't package, so they are **deferred and
documented**: `TRACY_LIBUNWIND_BACKTRACE` (libunwind), `TRACY_DEBUGINFOD`
(libdebuginfod), `TRACY_ROCPROF_*` (ROCm), Fortran, and the Python bindings.

How the defines combine: `:client` gets
`defines = select(enable) + select(on_demand) + ...`. Because they are
`defines` (which propagate) and not `local_defines`, every translation unit
that includes `Tracy.hpp` sees the same configuration. That matches CMake's
`PUBLIC` compile definitions.

## 4. Tests (`tests/client/`)

| Test | Checks |
| --- | --- |
| `disabled_test` | Default flags: macros compile to no-ops; the binary does not open a listening socket |
| `enabled_test` | Under a test-only transition to `enable=True`: zones, frames, plots, messages and the C API compile and run; `TracyIsConnected` is false and the program exits |
| `shared_two_dso_test` | Two `cc_shared_library`s plus a binary, all on `:client_dynamic`; asserts there is one profiler instance (a single `GetProfiler()` address) |
| `flags_build_test` | `build_test` of `:client` under a matrix of non-default flags (`on_demand`, `no_sampling`, `fibers`, `manual_lifetime`, `callstack=8`) |

A small `with_tracy_flags` transition rule (in `tests/`, not in the module)
lets one CI invocation test several configurations without command-line
flags.

## 5. Exit criteria

- `bazel build @tracy//:client @tracy//:client_shared` and
  `bazel test //tests/client/...` pass on all 6 native jobs.
- All 30 cross jobs pass `bazel build //... @tracy//:all`.
- The Pages publish workflow serves the registry, and an external scratch
  project can depend on `tracy` 0.15.0 from it.
- `source.json` hashes verified; the two `MODULE.bazel` copies are identical.

## 6. Risks

- `TracyClient.cpp` includes every `.cpp` under `public/client`. On Windows
  arm64 MinGW, `TracyCpuid`/`__cpuid` paths and the `dbghelp` symbol
  resolution may not compile; fall back to `TRACY_NO_CALLSTACK` for that cell
  if needed, and record why.
- Thread-safety warnings from the `@llvm` toolchain: Tracy is a
  non-`external/(?!tracy)` repo, so give it a targeted
  `per_file_copt=external/tracy[+]/.*@-Wno-thread-safety-analysis` rather
  than `-w`.

## 7. As built (2026-11-16)

Where the implementation differs from the plan above, and why:

- **One config rule, not one `select()` per flag.** `config/tracy_config.bzl`
  defines a `tracy_config` rule that reads every build setting and returns
  `CcInfo` with the resulting `defines`. `:headers` depends on it. This
  handles the int (`callstack`) and string (`platform_header`) settings the
  same way as the booleans. It also enforces upstream's
  `disallow_hw_timer` ⇒ `timer_fallback` rule.
- **`platform_header`** is a `string_flag` (the include path) plus a
  `label_flag` `platform_header_lib` (the `cc_library` providing it),
  because a define cannot carry a label.
- **Windows linkopts** follow upstream's MinGW branch: `ws2_32 dbghelp
  secur32`, plus `advapi32 user32`, which MSVC would get from
  `#pragma comment`.
- **Tests** (`tests/client/`) use `with_cfg.bzl` to build rules per
  configuration:
  - `disabled_test`;
  - `enabled_test` (C++ and C API, threads, locks, allocations);
  - `enabled_{on_demand,minimal,fibers,timer_fallback}_test`;
  - `manual_lifetime_test`;
  - `shared_two_dso_test` (asserts that one profiler address is seen from
    the executable and both plugin DLLs).

  The planned `flags_build_test` became the variants above, which also run.
- **`//platforms:host_test_toolchain`** (root only, dev dependency): the test
  rules generated by `with_cfg` keep Bazel's default `test` exec group. Its
  built-in toolchain demands an execution platform with *all* target
  constraints, and none carries the `libc:gnu.2.34` pin from
  `//platforms:linux`.
- **New `@llvm` patch** `windows_static_libcxxabi_no_exports.patch`: the static
  Windows libc++abi was compiled without `_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS`,
  so its objects carried `-export:` directives for `std::terminate`,
  `operator new`, and so on. Every DLL re-exported them, and lld rejected a
  second DLL linking against the first ("std::terminate() was replaced").
  This is the same root cause the Qt rules module works around with
  `win_cxxabi_shim`.
- **Windows DLL names.** With the MinGW toolchain, `cc_shared_library`
  defaults to `lib<name>.so`, which lld's `-l<name>` does not find.
  `:client_shared` sets `shared_lib_name` per OS; consumers must do the same
  on Windows (documented in the README).
- **Tracy module patch** `patches/mingw_arm64_cntvct.patch`: under MinGW on
  Windows/arm64, `_ReadStatusReg` and `ARM64_CNTVCT_EL0` do not exist (they
  are MSVC-only). The patch reads the counter with `mrs cntvct_el0` and
  skips the constant cross-check. Worth sending upstream.
- **`tools/sync_overlay.py`** keeps `source.json` hashes and the
  `MODULE.bazel` copy in sync. The Bazel server caches `file://` registry
  contents, so run `bazel shutdown` after changing a local module.

Verified locally on a linux-amd64 host:
- all 8 tests pass natively;
- `--config=tracy` builds for `linux_arm64_gnu.2.34`, `macos_arm64`,
  `macos_amd64`, `windows_amd64` and `windows_arm64`;
- an external workspace that uses only the registry and the host's default
  (non-`@llvm`) toolchain builds `@tracy//:client` and `:client_shared`,
  with and without `enable`, and runs an instrumented binary.

Not yet verified: native runs on macOS and Windows, and the Bazel `last_rc`
stage. Both are covered by the CI matrix once the repository is pushed.
