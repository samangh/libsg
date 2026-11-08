## Building from source

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

To produce a coverage report (GCC), add `-DCOVERAGE=ON` and build the
`coverage` target.

## Build options

Project specific options `CMakeLists.txt`:

| Option                    | Default | Description                                                                  |
|---------------------------|---------|------------------------------------------------------------------------------|
| `LIBSG_NET`               | `ON`    | Build the networking library (`sg::net`).                                    |
| `LIBSG_TLS`               | `ON`    | Enable TLS support in `sg::net`, using OpenSSL.                              |
| `LIBSG_IMGUI`             | `ON`    | Build the optional ImGui helpers (`sg::imgui`).                              |
| `LIBSG_IMGUI_DIRECTX`     | `ON`    | Enable the DirectX ImGui backend (Windows only).                             |
| `LIBSG_IMGUI_OPENGL`      | `ON`    | Enable the OpenGL ImGui backend (`OFF` by default on Windows).               |
| `LIBSG_ZSTD`              | `ON`    | Build the zstd compression helpers.                                          |
| `LIBSG_STACKTRACE`        | `OFF`   | Allow `sg::debug::exception_trace_mode::stacktrace` (needs Boost.Stacktrace). |
| `LIBSG_BUILD_TESTING`     | `ON`    | Build the Catch2 test suite (off when a libsg is included as a sub-project). |
| `LIBSG_BUILD_DOCS`        | `OFF`   | Generate Doxygen docs (only honoured as a sub-project).                      |

Additional options (from `cmake/DefaultPreamble.cmake`)

| Option                   | Default              | Description                                                                                    |
|--------------------------|----------------------|------------------------------------------------------------------------------------------------|
| `BUILD_SHARED_LIBS`      | `ON`                 | Build shared libraries (set `OFF` for static).                                                 |
| `USE_STATIC_LIBS`        | `!BUILD_SHARED_LIBS` | Prefer `.a` over `.so`/`.dylib` when locating external libraries.                              |
| `USE_STATIC_RUNTIME`     | `USE_STATIC_LIBS`    | Statically link the C++ runtime (MSVC only).                                                   |
| `IPO`                    | `OFF`                | Enable interprocedural / link-time optimisation if the toolchain supports it.                  |
| `ARCH_NATIVE`            | `OFF`                | Add `-march=native` (non-MSVC) or the equivalent SSE/AVX flags.                                |
| `USE_SSE`                | `ARCH_NATIVE`        | Enable SSE4.2 / AVX2 (+ CLMUL on x86-64) globally where supported.                             |
| `USE_LIBC++`             | `OFF`                | Build against `libc++` (Clang only).                                                           |
| `USE_LINTING`            | `OFF`                | Run `clang-tidy` on every target via `CXX_CLANG_TIDY`.                                         |
| `SANITIZE`               | `OFF`                | Turn on address + UB sanitizers (also exposes `SANITIZE_THREAD` / `SANITIZE_MEMORY`).          |
| `COVERAGE`               | `OFF`                | Add `-fprofile-instr-generate -fcoverage-mapping` and create a `coverage` target (Clang only). |
| `OWN_FMT`                | `OFF`                | Fetch & build `{fmt}` via CPM instead of using a system package.                               |
| `BUILD_DOCS`             | `OFF`                | Generate Doxygen documentation (requires Doxygen).                                             |
| `BUILD_DOCS_SRC`         | `OFF`                | Also scan source files when generating docs (requires `BUILD_DOCS`).                           |

There maybe other CMake options available, consult [this file](cmake/DefaultPreamble.cmake).