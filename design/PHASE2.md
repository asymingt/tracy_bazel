# Phase 2: Server library and CLI tools

Parent: [OVERVIEW](OVERVIEW.md). Depends on: [PHASE1](PHASE1.md).

## 1. Objective

Build the trace-processing core (`TracyServer`) and the seven headless tools,
so traces can be captured, converted, merged and exported in CI. Prove the
whole chain with an end-to-end capture test.

## 2. New dependency modules

### 2.1 `capstone` `6.0.0-alpha11`

- Source: `capstone-engine/capstone` tag `6.0.0-Alpha11`. The registry
  version spelling (`6.0.0-alpha11` vs `6.0.0-Alpha11`) gets confirmed against
  BCR version rules.
- Overlay: `cc_library(name = "capstone")` with all architectures enabled
  by default. A general-purpose BCR module should not hard-code Tracy's
  subset. Tracy disables most architectures and AT&T syntax only to save
  binary size, which doesn't affect correctness.
- Upstream exposes headers as `include/capstone/*.h`. Tracy includes them as
  `<capstone.h>`, through CMake's `include/capstone` include directory. So
  the module exports both `include` and `include/capstone` via `includes`.
- License: BSD-3-Clause (`LICENSES/LICENSE.TXT`; check the exact path).

### 2.2 `ppqsort` `1.0.6`

- Header-only (`include/ppqsort.h` + `include/ppqsort/*`).
- Tracy's two patches:
  - `ppqsort-semaphore.patch`: `binary_semaphore` → `counting_semaphore`.
    This fixes a real assertion bug, so it ships as a module patch with an
    upstream issue link.
  - `ppqsort-nodebug.patch` (`#ifndef NDEBUG` → `#if 0`) changes behaviour
    rather than fixing a bug. It should **not** go into the module; Tracy
    defines what it needs instead (to verify: the guard is debug-only
    checking).
- Check first whether a newer PPQSort release already includes the semaphore
  fix. If one does, package that version instead.

## 3. `@tracy` additions

| Target | Kind | Sources / deps |
| --- | --- | --- |
| `:common` | `cc_library` (private) | `public/common/{tracy_lz4,tracy_lz4hc,TracySocket,TracyStackFrames,TracySystem}.cpp` |
| `:server` | `cc_library` | `server/*.cpp`; deps `:common`, `@capstone`, `@zstd`, `@ppqsort`; `includes` for `public/common` and `server` |
| `:getopt`, `:dtl`, `:util` | `cc_library` (private) | `getopt/`, `dtl/` (headers), `cmake/util.cmake` sources |
| `:git_ref` | generated header | `GitRef.hpp` containing `namespace tracy { static inline const char* GitRef = "v0.15.0"; }`; the version string comes from `module_version()` |
| `:capture`, `:capture_daemon`, `:csvexport`, `:import_chrome` (+ `@nlohmann_json`), `:import_fuchsia`, `:update`, `:merge` | `cc_binary` | upstream output names kept via aliases (`tracy-capture`, ...) |
| `config:no_statistics` | `bool_flag` | `TRACY_NO_STATISTICS` on `:server` (upstream `NO_STATISTICS`) |

ISA: upstream compiles the server with `-march=native` unless
`NO_ISA_EXTENSIONS` is set. We never pass it. If performance matters later,
add an opt-in `config:server_isa=x86-64-v3` string flag that is only valid on
amd64. Leave it out of scope for now.

## 4. Tests

| Test | Checks |
| --- | --- |
| `tests/capstone/` | Disassembles a fixed x86-64 and AArch64 byte sequence |
| `tests/ppqsort/` | Parallel sort of 1M ints equals `std::sort` |
| `tests/tools/help_test` | Each tool binary runs `--help`/no args and exits with the expected code (catches link and startup failures on every native host) |
| `tests/e2e/capture_test` | `sh_test`/`py_test`: start `workload` (client with `enable=True`, `no_exit=True`, `only_localhost=True`, a fixed port via `TRACY_PORT`), run `tracy-capture -a 127.0.0.1 -p <port> -o out.tracy -s 3`, then `tracy-csvexport out.tracy`, and assert the expected zone names and counts |
| `tests/e2e/roundtrip_test` | `tracy-update` (recompress with zstd) and `tracy-merge` on `out.tracy`; `tracy-import-chrome` on a checked-in Chrome trace JSON; csvexport stays consistent |

The end-to-end test is the most valuable test in the project. It checks the
client and server against each other on every native host, which no
upstream CI does for this toolchain.

## 5. Exit criteria

- `capstone` and `ppqsort` modules each pass their smoke test across the
  full matrix (6 native + 30 cross).
- All seven tools build across the full matrix. Help and end-to-end tests pass
  on the 6 native jobs; the end-to-end test may be tagged off for a specific
  host only with a written justification.

## 6. Risks

- The capstone 6 alpha's code generation and file layout differ from 5.x.
  The overlay must list arch sources per `CAPSTONE_HAS_*`. Expect a large
  `BUILD.bazel`.
- Port collisions between parallel tests: use a port chosen at random per
  test, passed to both processes.
- The Windows end-to-end test needs Winsock loopback without firewall
  prompts on hosted runners; fall back to `tags = ["no-windows-ci"]` if it
  is flaky.
