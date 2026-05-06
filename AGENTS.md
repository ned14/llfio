# Agentic coding guidelines for llfio

The main git repository for this project is at https://github.com/ned14/llfio.

## Core rules

1. All source, header and test files are written for **C++ 17**. The CMake
   enforces this (see `CMakeLists.txt:184`). Do not use later standards
   unless guarded by feature-test macros. Where using a latest ISO C++
   standard feature would make sense, try to use the existing preprocessor
   macro infrastructure. If unsure, ask for direction.
2. Run `clang-format` on every changed header and source file. Do NOT run
   `clang-format` on cmake files.
3. When building and testing locally, follow the "Reconfigure, build, test"
   section below. For CI behavior, see `.ci.cmake` and the workflow files.
4. The `attic/` and `single-header/` directories are legacy/auxiliary work.
   You normally never need to touch them.
5. Do not read files unless you need them for the current task.
6. After completing a task, reflect on what would have reduced file reads or
   requests, and update this `AGENTS.md` accordingly.
7. The dual include tree (`include/llfio/` mirrors `include/llfio/v2.0/`) exists
   for backwards compatibility. New implementations belong in `v2.0/`; the
   top-level headers re-export from there. Do not edit the top-level mirrors.

---

### Repository layout

```
include/llfio/          → main public API (v2.0/ is current, non-versioned is legacy)
include/llfio/v2.0/     → current versioned API (what new code should use)
include/llfio/v2.0/detail/ → internal implementation (avoid unless modifying core)
test/tests/             → unit tests organised by feature subdirectory
example/                → compile-test examples
programs/               → standalone benchmarks/utilities (separate CMake project)
cmake/                  → build configuration, test declarations
```

---

## Quick start for common tasks

### Reconfigure, build, test (from repo root)
```bash
rm -rf build && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build _hl -j$(nproc)     # builds headers-only test/example suite
cmake --build _sl -j$(nproc)     # builds static library tests
cmake --build _dl -j$(nproc)     # builds shared library tests
ctest -R llfio_hl --output-on-failure
```
The targets `_hl`, `_sl`, `_dl` are meta-targets building all tests for the
header-only, static, and shared library variants respectively.

For CI-like behavior: `ctest -S .ci.cmake` (builds `_hl _sl _dl`, runs tests
with exclusions, packages artifacts). See "CI workflow quick-reference" below.

### Adding a new unit test
1. Create a file in `test/tests/` named `my_feature.cpp` (see `test/test_kernel_decl.hpp` for macros).
2. Add its path to `llfio_TESTS` in `cmake/tests.cmake` (hand-curated section).
3. Reconfigure; it appears as `llfio_hl--my_feature` etc.

### Adding a new example program (compile test)
1. Create `example/my_example.cpp`.
2. Add its path to `llfio_COMPILE_TESTS` in `cmake/tests.cmake` (hand-curated section).
3. Reconfigure; it appears as `example_my_example` (note the `example_` prefix).

### Disabling stale auto-scanned sources
If you add/remove source/header/test/example files and cmake cache doesn't update:
```bash
rm -f build/cmake/{headers,sources,interface,tests}.cmake.cache
# or delete the entire build/ directory
```
Then re-run cmake. The cached versions are recreated automatically.

### Common code locations

- File IO types: `include/llfio/v2.0/file_handle*.hpp`
- Memory mapping: `include/llfio/v2.0/mapped_file_handle*.hpp`
- Async operations: `include/llfio/v2.0/async_*.hpp`, `v2.0/algorithm/async_*`
- Directory operations: `include/llfio/v2.0/directory_handle*.hpp`
- Path utilities: `include/llfio/v2.0/path*.hpp`
- Error handling types: `include/llfio/v2.0/status*.hpp`, `v2.0/result_reference.hpp`

---


## Build and CI configuration

### Key CMake options (defined in CMakeLists.txt lines 42-55)
- `LLFIO_BUILD_EXAMPLES` (default `ON`) — controls whether `example/*` sources are built as compile tests.
- `BUILD_TESTING` (CMake builtin) — master switch for all tests and examples.
- `LLFIO_USE_EXPERIMENTAL_SG14_STATUS_CODE` (default `OFF`) — uses SG14 status_code for failure handling instead of `std::error_code`.
- `LLFIO_ENABLE_DEPENDENCY_SMOKE_TEST` (default `OFF`) — builds a smoke test executable for package managers.
- `LLFIO_FORCE_CONCEPTS_OFF` (default `OFF`) — disables automatic C++ concepts detection.
- `LLFIO_FORCE_COROUTINES_OFF` (default `OFF`) — disables automatic C++ coroutines detection.
- `LLFIO_FORCE_MAPPED_FILES_OFF` (default `OFF`) — disables memory mapped file support.
- `LLFIO_FORCE_SIGNAL_DETECTION_OFF` (default `OFF`) — disables signal raise detection.
- `UNIT_TESTS_BUILD_ALL` (default `OFF`) — builds all unit tests with the default `_hl` target instead of a subset.
- `UNIT_TESTS_CXX_VERSION` (default `"latest"`) — C++ standard to use for unit tests (`17`, `20`, `23`, `26` or `latest`).
- `LLFIO_USE_LIBDISPATCH` (default `ON` on BSD/Mac, `OFF` elsewhere) — whether to use libdispatch/Grand Central Dispatch.

### Dependencies (fetched via git at configure time)
- `quickcpplib` — build system and general utilities (header-only)
- `outcome` — failure handling library (header-only)
- `kerneltest` — test kernel harness for unit tests (header-only)
- `ntkernel-error-category` — Windows-only, Windows NT kernel error codes (git submodule under `include/llfio/`)
- `wg14_signals` — POSIX signal handling (git submodule under `include/llfio/`)

### quickcpplib dependency map

`quickcpplib` is cloned into the build directory at configure time
(`build/quickcpplib/repo/`). Its CMake modules are added to `CMAKE_MODULE_PATH`,
making these key functions/macros available:

| Symbol | Purpose | Location in quickcpplib |
|--------|---------|------------------------|
| `find_quickcpplib_library()` | Fetch/build a quickcpplib dependency (auto-clones if missing) | `cmakelib/FindQuickCppLibLibrary.cmake` |
| `QuickCppLibMakeStandardTests` | Generate `_hl/_sl/_dl` test targets from `llfio_TESTS` etc. | `cmakelib/QuickCppLibMakeStandardTests.cmake` |
| `QuickCppLibBootstrap.cmake` | Bootstraps quickcpplib's CMake tooling (included by this project) | `cmake/QuickCppLibBootstrap.cmake` (mirrored from quickcpplib) |
| `all_link_libraries()` | Convenience wrapper for target linking | `cmakelib/AllLinkLibraries.cmake` |

**Avoid exploring `quickcpplib/repo/` manually** — the module paths above directly
identify what you need. The `include/` subdirectory of each quickcpplib library
(e.g., `quickcpplib/repo/quickcpplib/include/`) provides header-only utilities.

The `kerneltest` test framework (used by unit tests via `#include "test/test_kernel_decl.hpp"`)
originates from quickcpplib's `kerneltest` component.

### Test organization
- `cmake/tests.cmake` declares `llfio_TESTS`, `llfio_COMPILE_TESTS`, `llfio_COMPILE_FAIL_TESTS`.
- `QuickCppLibMakeStandardTests` (from `quickcpplib` dependency) generates `llfio_TEST_TARGETS`, `llfio_COMPILE_TEST_TARGETS` from those lists.
- `llfio_EXAMPLE_TARGETS` is derived from `llfio_COMPILE_TEST_TARGETS` by matching `example/` in source paths (CMakeLists lines 336-348).
- To disable a subset of sources: filter the appropriate `llfio_*` list with `list(FILTER ... EXCLUDE REGEX "prefix")` before `include(QuickCppLibMakeStandardTests)`.

**Important**: All three test lists must be manually edited in `cmake/tests.cmake`. The file-wide `# DO NOT EDIT, GENERATED BY SCRIPT` markers are misleading — the list contents are hand-maintained.

#### Test structure

Tests reside in `test/tests/<feature>/` with two files:
- `runner.cpp` — sets up the `kerneltest` framework and dispatches to kernels
- `<feature>_*.hpp` — shared kernel test definitions used by multiple runners

Common shared kernels live in subdirectories under `test/tests/` (e.g., `file_handle/`, `mapped_file/`). See `test/test_kernel_decl.hpp` for the macros.

### Test target naming
- Regular tests: `target--testname` (e.g., `llfio_hl--mapped`)
- Special build tests (asan/ubsan/msan/tsan/sa): `target-special-testname`
- Compile tests from `example/`: `example_my_example` (prefix rewritten from `llfio_hl--my_example`)

All test/compile-test targets have `EXCLUDE_FROM_ALL ON`. Build via `_hl`/`_sl`/`_dl` meta-targets or individual target names.
`COMPILE_FAIL_TESTS` also get `EXCLUDE_FROM_DEFAULT_BUILD ON`; their test runs `cmake --build . --target <name>` to intentionally fail.
`UNIT_TESTS_BUILD_ALL=ON` sets `_hl`/`_sl`/`_dl` to `EXCLUDE_FROM_ALL OFF` so all tests build with `make all`.
`llfio_TESTS_DISABLE_PRECOMPILE_HEADERS` — regex list of target names to skip PCH reuse (currently `coroutines` tests only).

### CI workflow quick-reference
- **Linux**: `.github/workflows/unittests_linux.yml` — matrix over `compiler: [clang++, g++, libc++, arm-linux-gnueabihf-g++]` and `configuration: [error_code, status_code]`.
- **macOS**: `.github/workflows/unittests_macos.yml` — matrix over `configuration: [error_code, status_code]`. Excludes `noexcept` tests.
- **Windows**: `.github/workflows/unittests_windows.yml` — matrix over image `[windows-2022]` and `configuration`, plus a MinGW job using Ninja.
- **Programs** (benchmarks/utilities in `programs/`): `.github/workflows/programs.yml` — builds each program as a standalone CMake project.

### How CI options propagate
```
.github/workflows/unittests_*.yml (sets CMAKE_CONFIGURE_OPTIONS)
    ↓ passed to
ctest -S .ci.cmake -DCTEST_CONFIGURE_OPTIONS="..."
    ↓ read by
.ci.cmake: ctest_configure(OPTIONS "${CTEST_CONFIGURE_OPTIONS}")
    ↓ then
ctest_build(TARGET _hl | _sl | _dl)
ctest_test(EXCLUDE "...")
```
Append options with semicolon-separated `-DNAME=VALUE` entries (bash string).

### Important non-obvious facts
- `programs/` has its own `CMakeLists.txt`; it is built as a standalone project via the programs workflow, NOT via `add_subdirectory` from the top-level project.
- `include/llfio/revision.hpp` is auto-updated by `UpdateRevisionHppFromGit()` at every configure commit (CMakeLists:70). Do not edit manually; it resets to HEAD on checkout.
- `cmake/` cache files (`headers.cmake`, `sources.cmake`, `interface.cmake`, `tests.cmake.cache`) store scanned source lists. Delete them (or the whole `build/`) to force a fresh scan.
- `llfio_COMPILE_FAIL_TESTS` use `cmake --build . --target` to intentionally fail; their success condition matches a regex from the source file's second line.
- The `_hl`/`_sl`/`_dl` default dependency: `_sl` or `_hl` becomes the default `all` build target depending on `BUILD_SHARED_LIBS`. The other has `EXCLUDE_FROM_ALL ON`.
- Unit tests use the `kerneltest` framework (via `#include "test/test_kernel_decl.hpp"`). The test runner pattern lives in subdirectories like `test/tests/file_handle_create_close/runner.cpp` with shared `.hpp` kernels.
- Special sanitizer builds (`asan`, `ubsan`, `msan`, `tsan`, `sa`) run the full test suite under those sanitizers automatically when available.
- Windows-only targets link against `ntkernel-error-category` automatically unless using SG14 status_code.
- The header-only unit test suite auto-generates `--noexcept` and (MSVC only) `--permissive` variant targets from each regular test source. These build with exceptions/RTTI disabled.
- The `llfio_hl` target compiles with precompiled headers enabled for faster builds. Specific targets can opt out via `llfio_TESTS_DISABLE_PRECOMPILE_HEADERS`.

### Pattern: Disabling a source-based subset for specific CI configurations
Used for `LLFIO_BUILD_EXAMPLES`; reusable for `example/`, `test/`, or any custom-filtered set:
1. Add boolean option `LLFIO_BUILD_<SUBSET>` to the options block in `CMakeLists.txt`.
2. Insert after `set(llfio_TESTS_DISABLE_PRECOMPILE_HEADERS ...)` (before `include(QuickCppLibMakeStandardTests)`, around line 246):
   ```cmake
   if(NOT LLFIO_BUILD_<SUBSET>)
     list(FILTER llfio_<LIST> EXCLUDE REGEX "^prefix/")
   endif()
   ```
   Where `<LIST>` is `COMPILE_TESTS` (for examples), `TESTS` (for normal unit tests), etc.
3. In CI, set `-DLLFIO_BUILD_<SUBSET>=OFF` in `CMAKE_CONFIGURE_OPTIONS`.
4. Filter pattern matches the path prefix as stored in the variable (e.g., `^example/`, `^test/` or `^test/tests/slow_`).

### Platform-specific notes
- **Linux**: `LLFIO_FORCE_OPENSSL_OFF=On` may be needed for cross-compilation (seen in arm CI job).
- **MacOS**: `noexcept` tests are excluded (`ctest -E noexcept`) due to CI resource limits.
- **Windows**: MinGW uses `Ninja` generator; VS2022 uses `Visual Studio 17 2022`. MSVC gets extra flags (`/wd4503`, `/permissive-`).
- **WSL**: Some tests short-circuit when `utils::running_under_wsl()` returns true.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Option appears ignored | CMake cache retains old value | Delete `build/` or use `cmake -U<var>` |
| New source not picked up | `QuickCppLibParseLibrarySources` caches listings | Clear `cmake/*.cmake.cache` or delete `build/` |
| "Cannot find quickcpplib" | Network blocked / submodule not cloned | Place a clone at `build/quickcpplib/repo/` or set `CTEST_QUICKCPPLIB_CLONE_DIR` to a local path |
| Example still builds despite `LLFIO_BUILD_EXAMPLES=OFF` | `llfio_COMPILE_TESTS` still contains `example/...` entries | Verify the filter matched; check `cmake/tests.cmake` |
| Building wrong default target | `BUILD_SHARED_LIBS=ON` flips default from `_hl` to `_sl` | Check `CMakeCache.txt` |
| Revision header keeps changing | `UpdateRevisionHppFromGit` runs at every configure | Normal behavior; do not edit |
| Don't know where to find a symbol | Unfamiliar codebase layout | Use the grep shortcuts in "Finding code quickly" below; browse by directory (see "Repository layout") |

### Finding code quickly

Instead of `grep`-ing the whole repo:
- Find handle types: `grep -r "LLFIO_V2_0_INLINE_VARIABLE .*_handle" include/llfio/v2.0/`
- Find free functions: `grep -r "LLFIO_V2_0_INLINE_VARIABLE" include/llfio/v2.0/ | grep -v "class\|struct"`
- Browse by directory (see "Repository layout" above)

---

## What the test lists mean

- `llfio_TESTS` — regular unit test executables running the `kerneltest` framework.
  Tests live in `test/tests/*.cpp` plus subdirectories with runner+kernel patterns.
- `llfio_COMPILE_TESTS` — compile-only test programs (no executable). Used to
  verify examples and edge-case compilation. Sources live under `example/`.
- `llfio_COMPILE_FAIL_TESTS` — sources that must fail to compile (currently empty).

You add to these lists by editing `cmake/tests.cmake` directly. They are **not**
auto-generated despite the file's "DO NOT EDIT" markers — verified by inspection.

---

## Dependency management

Dependencies (`quickcpplib`, `outcome`, `kerneltest`) are fetched at configure
time via `find_quickcpplib_library()`. The `ntkernel-error-category` and
`wg14_signals` libraries are git submodules under `include/llfio/`.

```
git submodule update --init --recursive
```
is run automatically by `ensure_git_subrepo()` during configuration, so a
regular `git clone` suffices.

---

## Reference

- **Build docs**: [Build.md](Build.md)
- **Project README**: [Readme.md](Readme.md)
- **CI configuration**: `.ci.cmake`, `.github/workflows/`
- **CMake modules**: `cmake/QuickCppLibBootstrap.cmake` (loads rest from `quickcpplib`)
- **Test kernel**: `test/test_kernel_decl.hpp` (macros for writing tests)
- **Scripts**: `scripts/` (various utility scripts for releases, multi-abi testing, etc.)
