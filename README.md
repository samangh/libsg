[![Windows](https://github.com/samangh/libsg/actions/workflows/windows.yml/badge.svg)](https://github.com/samangh/libsg/actions/workflows/windows.yml)
[![MSYS2](https://github.com/samangh/libsg/actions/workflows/msys2.yml/badge.svg)](https://github.com/samangh/libsg/actions/workflows/msys2.yml)
[![Linux](https://github.com/samangh/libsg/actions/workflows/linux.yml/badge.svg)](https://github.com/samangh/libsg/actions/workflows/linux.yml)
[![macOS](https://github.com/samangh/libsg/actions/workflows/macos.yml/badge.svg)](https://github.com/samangh/libsg/actions/workflows/macos.yml)
[![codecov](https://codecov.io/gh/samangh/libsg/graph/badge.svg?token=ZAL8QI6GQR)](https://codecov.io/gh/samangh/libsg)

# libsg

A modern C++20 utility library bundling networking, concurrency primitives,
buffers, file I/O, compression, and assorted helpers behind a single
`SG::common` CMake target. Tested on Linux, macOS, Windows (MSVC), and MSYS2.

## Requirements

- A C++20 compiler (GCC, Clang, AppleClang, MSVC).
- CMake 3.15 or newer.
- [Boost](https://www.boost.org) (headers; `Boost::stacktrace` optional).
- [{fmt}](https://github.com/fmtlib/fmt).
- [libuv](https://libuv.org).
- Optional: [zstd](https://github.com/facebook/zstd) for compression support.

## Installation

The recommended way to consume `libsg` is via
[CPM](https://github.com/cpm-cmake):

```cmake
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(MyProject CXX)

file(DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/get_cpm.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

CPMAddPackage(
  NAME libsg
  GITHUB_REPOSITORY samangh/libsg
  GIT_TAG master
  GIT_SHALLOW
  GIT_SUBMODULES_RECURSE ON
  OPTIONS
    "LIBSG_BUILD_TESTING OFF"
    "LIBSG_IMGUI OFF")

add_executable(main main.cpp)
target_link_libraries(main PRIVATE SG::common)
```

## Build Options

| Option                     | Default | Description                                              |
| -------------------------- | ------- | -------------------------------------------------------- |
| `LIBSG_IMGUI`              | `ON`    | Build the optional ImGui helpers (`SG::imgui`).          |
| `LIBSG_ZSTD`               | `ON`    | Build the zstd compression helpers.                      |
| `LIBSG_STACKTRACE`         | `OFF`   | Attach stack traces to `SG_THROW` exceptions.            |
| `LIBSG_EXCEPTION_DETAILS`  | `OFF`   | Attach function/file/line info to `SG_THROW` exceptions. |
| `LIBSG_BUILD_TESTING`      | `ON`*   | Build the Catch2 test suite (*off when sub-project).     |

## What's in the library

All public headers live under `sg/`. Most types are in the `sg::` namespace,
with subsystem-specific namespaces (`sg::net`, `sg::compression::zstd`, etc.).

### Networking (`sg::net`)
- `tcp_server` — accept-loop based server with callback-driven session model.
- `tcp_client` — asynchronous TCP client sharing a session abstraction.
- `tcp_client_sync` — blocking client with `read_until` / `read_some` helpers.
- `tcp_session` — per-connection object exposing read/write/disconnect.
- `asio_io_pool` — restartable thread pool driving a single `boost::asio::io_context`.
- `end_point`, `keepalive_t`, `interfaces()`, `resolve(...)`.

### Concurrency
- `worker` — interval-driven background thread with start/tick/stop callbacks and
  on-demand `notify()`.
- `state_machine<TState>` — generic state machine layered on top of `worker`.
- `jthread.h` — wrapper that uses `std::jthread` when available, polyfills it otherwise.

### Buffers and memory
- `IBuffer<T>` interface and the `unique_buffer` / `shared_buffer` /
  `unique_opaque_buffer` / `shared_opaque_buffer` family, with type-erased
  deleters (including `deleter_free` for C-allocated memory).
- `rolling_contiguous_buffer<T>` — circular buffer with contiguous storage.
- `enable_lifetime_indicator`, `pimpl<T>` helpers.

### Data channels (`sg::data`)
- `channel`, `channel_vector`, `channel_rolling`, `channel_compressed` — named,
  typed data streams sharing a common `IChannelBase` interface.

### I/O
- `sg::common::file::read` / `write` for whole-file buffer I/O.
- `file_writer` — append-only writer with an async queue and dedicated thread.

### Compression (`sg::compression::zstd`)
- One-shot `compress` / `decompress` over raw pointers, contiguous ranges or
  `IBuffer<std::byte>`.

### Utilities
- `sg::checksum::crc32`, `crc32c` (with hardware fast path), `crc16`.
- `sg::uuids::uuid` — value type around `boost::uuids::uuid`.
- `sg::version` — comparable dotted version.
- `sg::bytes` — `byteswap`, endian helpers.
- `sg::bounds` — `upper_bound_index`, `lower_bound_index` over raw arrays.
- `sg::format`, `sg::string`, `sg::ranges`, `sg::math`, `sg::map`.
- `sg::process` — process / thread enumeration.
- `sg::cpu` — vendor, model, parallelism estimate.
- `sg::locale` — UTF-8 ctype helpers.
- `sg::random::generate<T>(count)` — quick shuffled-iota vectors.

## Core concepts

### Tagged callbacks

Callbacks are declared with the `CREATE_CALLBACK` macro. Each call generates a
phantom tag type so that two callbacks with the same signature but different
meanings cannot be mistakenly swapped:

```cpp
#include <sg/callback.h>

CREATE_CALLBACK(on_connected_cb_t,  void(int session_id))
CREATE_CALLBACK(on_disconnect_cb_t, void(int session_id))

on_connected_cb_t cb = [](int id) { /* ... */ };
cb.invoke(42);   // use .invoke(), not operator()
```

### Buffers

`unique_buffer` and `shared_buffer` own a contiguous run of `T` with a
configurable deleter. Use the `_c_` variants when the memory was allocated with
`malloc` (or any function compatible with `free`):

```cpp
#include <sg/buffer.h>

auto buf = sg::make_shared_c_buffer<std::byte>(1024); // free()-backed
std::memcpy(buf.get(), source, 1024);
```

Use the `*_opaque_buffer` variants when you need to hand a buffer across an API
boundary without exposing the deleter type.

## Examples

### TCP echo server

```cpp
#include <sg/tcp_server.h>

using namespace sg::net;

int main() {
    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable =
        [](tcp_server& s, auto id, const std::byte* data, size_t size) {
            s.write(id, data, size);
        };

    tcp_server server;
    server.start({{"127.0.0.1", 55555}}, cb);
    server.future_get_once(); // wait until stopped
}
```

Add connection lifecycle callbacks the same way:

```cpp
cb.OnSessionCreated = [](tcp_server&, auto id) {
    std::cout << "client " << id << " connected\n";
};
cb.OnDisconnected = [](tcp_server&, auto id, std::exception_ptr ex) {
    std::cout << "client " << id << " disconnected\n";
};
```

### Synchronous TCP client

```cpp
#include <sg/tcp_client_sync.h>
using namespace sg::net;

tcp_client_sync client;
client.connect({"127.0.0.1", 55555});
client.write("hello\n");
auto reply = client.read_until("\n");
```

### Periodic background work

```cpp
#include <sg/worker.h>
using namespace std::chrono_literals;

sg::worker w(100ms,
    sg::worker::callbacks_t{
        .on_start_callback = [](sg::worker*) { /* setup */ },
        .on_tick_callback  = [](sg::worker*) { /* every 100ms */ },
        .on_stop_callback  = [](sg::worker*) { /* teardown */ },
    });
w.start();
w.notify();          // run the tick immediately
w.request_stop();
w.wait_for_stop();
```

### File writer

```cpp
#include <sg/file_writer.h>

sg::file_writer writer;
writer.start("log.bin",
             /*on_error*/  nullptr,
             /*on_start*/  nullptr,
             /*on_stop*/   nullptr);
writer.write_async("hello\n");
writer.stop();
```

### zstd compression

```cpp
#include <sg/compression_zstd.h>

std::vector<std::byte> input = /* ... */;
auto compressed = sg::compression::zstd::compress(
    input, sg::compression::zstd::default_compresssion_level(), /*threads*/ 1);
auto roundtrip  = sg::compression::zstd::decompress<std::byte>(
    compressed.get(), compressed.size());
```

### Custom exceptions

```cpp
#include <sg/exceptions.h>

namespace myapp::errors {
    SG_CREATE_EXCEPTION_ANY();                          // myapp::errors::any
    SG_CREATE_EXCEPTION(bad_config, "bad config");      // myapp::errors::bad_config
}

try {
    SG_THROW(myapp::errors::bad_config, "missing key 'port'");
} catch (const myapp::errors::any& e) { /* ... */ }
```

## Building from source

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

To produce a coverage report (GCC), add `-DCOVERAGE=ON` and build the
`coverage` target.

## Testing

Tests live in `test/` and use [Catch2](https://github.com/catchorg/Catch2).
They are built whenever `libsg` is the top-level project and discovered via
`catch_discover_tests`, so any standard CTest invocation will run them.

## License

See [LICENSE](LICENSE) and the [LICENCES](LICENCES) directory for third-party
notices.
