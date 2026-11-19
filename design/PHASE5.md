# Phase 5: Hardening and upstreaming

Parent: [OVERVIEW](OVERVIEW.md).

## 1. Objective

Make every module in `modules/` ready for the BCR, and leave the
workspace in a maintainable state, using what the Qt workspace learned in
`design/REVIEW.md`.

## 2. Work items

1. **`design/REVIEW.md` pass**, using the Qt checklist:
   - no canonical repo names;
   - no reliance on root-only overrides;
   - every transitive label backed by a direct `bazel_dep`;
   - `license_text` paths exist;
   - `presubmit.yml` targets build outside this workspace;
   - `metadata.json` complete;
   - hashes in `source.json` match.
2. **Build from outside the workspace.** A scratch project in CI that uses
   only `--registry=<pages URL>` + BCR, with no `.bazelrc` flags from this
   repository, builds `@tracy//:client`, `:capture` and `:profiler`. Any flag
   that turns out to be required gets documented in the README (as the Qt
   README does for `system_include_paths`) or removed.
3. **Sanitizers.** Run `--config=asan`, `ubsan` and `tsan` on
   `tests/client/...` and `tests/e2e/...` (Linux native).
   `tsan` is particularly relevant to the client's lock-free queues; tag
   known upstream false positives.
4. **Bazel `last_rc` stage** green and the status-matrix SVGs published;
   README written, following the Qt README structure (motivation, try it now,
   what's in the box, build status, licensing).
5. **Prune.** Remove `@llvm` patches and `osx.frameworks` entries that Tracy
   doesn't need. Bump `@llvm` if upstream has merged our patches.
6. **Windows polish.** Icon resource and DPI-aware manifest for
   `tracy-profiler.exe` via `llvm-rc`.
7. **BCR submission, leaf-first:**
   1. `capstone`, `ppqsort`, `md4c`, `base64`, `tidy-html5`, `usearch`,
      `wayland-protocols`;
   2. `dbus`, `libglvnd` (coordinated with the Qt workspace; done: both are in
      the BCR and were removed from `modules/` on 2026-11-18);
   3. `nativefiledialog-extended`, `imgui` 1.93.0-docking bump,
      `glfw` 3.5.1 bump (if needed);
   4. `tracy` 0.15.0.

   Send upstream projects the patches we carry (PPQSort semaphore, nfd
   xdg-foreign-v2 if still needed, imgui #9519 if not yet merged).

## 3. Explicitly out of scope (candidates for later)

- Python bindings (`TRACY_CLIENT_PYTHON`, pybind11), Fortran bindings.
- `TRACY_LIBUNWIND_BACKTRACE`, `TRACY_DEBUGINFOD`, ROCprof integration.
- Emscripten/WASM GUI build, macOS `.app` bundle (`BUNDLE`), GTK file
  selector.
- `-march`-tuned server builds.
- Tracking Tracy releases after 0.15.0. The version bump process gets
  documented in the README once 0.15.0 lands in the BCR.

## 4. Exit criteria

- The REVIEW.md findings are all resolved or explicitly accepted.
- The outside-workspace build is green.
- BCR PRs are filed for every module, with presubmit passing.

## 5. As built (2026-11-17)

1. **Review.** `design/REVIEW.md` records the full checklist:
   - no canonical labels;
   - every repo label is a `bazel_dep`;
   - licenses exist upstream;
   - metadata and hashes are consistent.

   It also has findings F1 to F6.
2. **Build from outside the workspace.** `.github/workflows/consumer.yaml`
   builds a fresh workspace on Ubuntu (GCC), macOS (Apple clang) and Windows
   (MSVC). It uses this checkout as a registry and the host toolchain, with no
   flags or overrides from this repo, and builds the client, the CLI tools and
   the GUI. GCC and Apple clang passed on the first run. MSVC exposed MinGW-only
   `-l` link flags, so every Windows link list in the overlays now carries both
   `-DEFAULTLIB:<lib>.lib` and `-l<lib>`.
3. **Sanitizers** (Linux):
   - ASan, UBSan and TSan are clean on the client, tools, ppqsort and base64
     tests.
   - The exceptions are upstream behaviours, documented in REVIEW F6 and
     tagged `noasan`/`noubsan`/`notsan`: ODR noise from the toolchain linking
     libc++ twice under ASan; unaligned `MemRead` and vendor DWARF tags under
     UBSan; Tracy's lock-free client internals and PPQSort's partition under
     TSan.
4. **Bazel `last_rc`.** The stage in `pull_request.yaml` runs once the 9.x
   stage is green. The README embeds the status matrix for both stages.
5. **Pruning.** `@llvm` was updated to 0.8.27, which drops all but one of the
   earlier `@llvm` patches. The first Windows-host cross run showed that
   `windows_pkg_dir_symlinks.patch` (shared with qt_bazel_workspace) is still
   needed for Windows -> macOS, and that Windows -> Linux needs
   `--nointerface_shared_objects` so long `.so` links use a `@params` file.
   Root-only items are listed in REVIEW F1.
6. **Windows icon and manifest:** deferred, because `rules_cc` has no portable
   resource-compiler action (REVIEW, "Deferred").
7. **Bugs found and fixed along the way:**
   - **#1:** the MinGW stack-guard initialisation race. It was diagnosed from a
     symbolised CI backtrace and is verified 40/40 on Windows.
   - **`windows_safe_file_write.patch`:** `SafeFileWrite::Commit()` renamed a
     still-open file, which fails on Windows (MSVC as well). It broke
     `tracy-update` and `tracy-merge` there, and was found by the end-to-end
     test on the Windows CI runners.
8. **CI status.** `4336733` is green across the board: 6 native and 30 cross
   jobs on both Bazel 9.x and `last_rc`, plus the Consumer workflow on Ubuntu
   (GCC), macOS (Apple clang) and Windows (MSVC). Fixes found on the way:
   - MSVC: Bazel quote-includes every repository root, and cl.exe treats
     quote includes as `/I`, so usearch's root `VERSION` shadowed `<version>`
     on a case-insensitive file system (`remove_version_file.patch`); capstone
     now declares `include/platform.h`.
   - Windows-host cross builds: `windows_pkg_dir_symlinks.patch` and
     `--nointerface_shared_objects` (REVIEW F1).
   - sway's IPC socket path must fit in `sun_path`, so the headless test uses a
     short runtime folder.
   - Bazel `last_rc` on Windows: retry on exit code 36 (REVIEW F7).
9. **BCR submission: deferred by the maintainer (2026-11-17).** No BCR PRs
   are filed yet, and `imgui` stays `1.93.0-docking.tracy.1` (REVIEW F3). The
   modules are ready: green in this workspace's matrix and in the Consumer
   workflow. The leaf-first order and the patches to send upstream are in
   REVIEW F1 and F2. This exit criterion stays open until submission is
   scheduled.
