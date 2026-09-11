# Host-side unit tests

These tests run on your laptop (not on the device) against the pure modules
in [`src/pure/`](../src/pure/) and the KV-store-backed modules in
[`src/storage/`](../src/storage/) (`bookmarks.cpp`, `list_items.cpp`).

They're a separate build from the firmware. The firmware build (`pio run`)
ignores this directory; CMake here ignores everything that depends on the
ESP32 toolchain.

> **Note:** PlatformIO's built-in test runner (`pio test`) also defaults to
> this directory. We don't use it — `platformio.ini` sets `test_ignore = *`
> so `pio test` is a no-op. Run tests with CMake / `ctest` as shown below.

## Requirements

- **CMake 3.13+**
- A **C++17 compiler** — MSVC, GCC, or Clang. CMake picks one automatically.

### Installing the prerequisites

**Windows**

- Install [Visual Studio 2019 or newer](https://visualstudio.microsoft.com/downloads/) with the *"Desktop development with C++"* workload (this gets you MSVC + the Windows SDK), **or** install [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) if you don't want the full IDE.
- Install CMake from [cmake.org/download](https://cmake.org/download/), or via `winget install Kitware.CMake` / `choco install cmake`. Make sure it's on `PATH`.
- Run the commands below from a regular PowerShell or `cmd` prompt — CMake will locate MSVC on its own. (You don't need to launch the "x64 Native Tools" shell.)

**Linux**

```bash
# Debian / Ubuntu
sudo apt install build-essential cmake

# Fedora
sudo dnf install gcc-c++ cmake make

# Arch
sudo pacman -S base-devel cmake
```

**macOS**

```bash
xcode-select --install        # Clang + make
brew install cmake            # CMake (or download from cmake.org)
```

## Run

From the repo root:

```bash
cmake -S test -B test/build                                 # configure (once, or after CMakeLists.txt changes)
cmake --build test/build --config Release                   # build
ctest --test-dir test/build -C Release --output-on-failure  # run
```

Or as a one-liner:

```bash
cmake -S test -B test/build && cmake --build test/build --config Release && ctest --test-dir test/build -C Release --output-on-failure
```

The `-C Release` flag is only meaningful on multi-config generators (Visual Studio on Windows) — it's harmless on single-config generators (Make / Ninja on Linux / macOS), so the same command line works everywhere.

For the verbose per-test output, run the binary directly:

```bash
test/build/Release/host_tests.exe     # Windows / MSVC
test/build/host_tests                 # Linux / macOS / MinGW
```

### The two binaries

`ctest` runs both:

- **`host_tests`** — the pure modules and the KV-store-backed ones.
- **`host_fs_tests`** — [`storage/sync_map.cpp`](../Pala_One_2_1/src/storage/sync_map.cpp),
  which reads a real file. It is a separate binary because it needs
  [`hostfs/`](hostfs) on the include path *ahead of* the firmware tree: that
  directory shadows `src/state.h` and `src/config.h` with stubs providing a
  `File` / `FS` over ordinary files, so the module does genuine seeks and
  short reads rather than talking to a mock. Shadowing those headers is
  incompatible with `host_tests`, which wants the real ones.

## What's covered

| Test file | Module under test |
|---|---|
| [`test_paths.cpp`](test_paths.cpp) | path / folder / filename sanitization |
| [`test_text_util.cpp`](test_text_util.cpp) | UTF-8 helpers + typography normalization |
| [`test_hashing.cpp`](test_hashing.cpp) | FNV-1a + per-book preference keys |
| [`test_paginator.cpp`](test_paginator.cpp) | line wrapping at words / punctuation / UTF-8 |
| [`test_bookmark_label.cpp`](test_bookmark_label.cpp) | first-words bookmark label |
| [`test_bookmarks_codec.cpp`](test_bookmarks_codec.cpp) | bookmark byte-blob encode/decode + v1 migration |
| [`test_list_codec.cpp`](test_list_codec.cpp) | todo-list byte-blob encode/decode |
| [`test_bookmarks_store.cpp`](test_bookmarks_store.cpp) | `loadBookmarks` / `saveBookmarks` against `MapKvStore` |
| [`test_list_store.cpp`](test_list_store.cpp) | `loadList` / `saveList` against `MapKvStore` |
| [`test_xpointer.cpp`](test_xpointer.cpp) | crengine / KOReader XPointer parse + compose |
| [`test_sync_map_codec.cpp`](test_sync_map_codec.cpp) | spine-map file format: header, section offsets, records |
| [`test_sync_map.cpp`](test_sync_map.cpp) | XPointer ⇄ byte offset against a real map file (`host_fs_tests`) |

## Adding a new test

1. Add a `test_<thing>.cpp` here. Mirror the source file you're testing.
2. List the source file (and the new test file) in
   [`CMakeLists.txt`](CMakeLists.txt) under `SUT_SRC` / `TEST_SRC`.
3. Use `TEST_CASE("name") { ... }` with `CHECK(expr)` / `CHECK_EQ(a, b)` /
   `REQUIRE(expr)` from [`test_framework.h`](test_framework.h).

If the module touches the filesystem, add it to the `host_fs_tests` target
instead and let it open real files under a `mkdtemp` root — see
[`test_sync_map.cpp`](test_sync_map.cpp).

The pure modules compile here because they include
[`src/pure/arduino_compat.h`](../src/pure/arduino_compat.h) — which provides
a minimal `String` shim when `ARDUINO` isn't defined. If you need a method
on `String` that isn't there yet, add it to the shim.
