[![Windows](https://github.com/samangh/libsg/actions/workflows/windows.yml/badge.svg)](https://github.com/samangh/libsg/actions/workflows/windows.yml)
[![MSYS2](https://github.com/samangh/libsg/actions/workflows/msys2.yml/badge.svg)](https://github.com/samangh/libsg/actions/workflows/msys2.yml)
[![Linux](https://github.com/samangh/libsg/actions/workflows/linux.yml/badge.svg)](https://github.com/samangh/libsg/actions/workflows/linux.yml)
[![macOS](https://github.com/samangh/libsg/actions/workflows/macos.yml/badge.svg)](https://github.com/samangh/libsg/actions/workflows/macos.yml)
[![codecov](https://codecov.io/gh/samangh/libsg/graph/badge.svg?token=ZAL8QI6GQR)](https://codecov.io/gh/samangh/libsg)

## How to use

You can include this project using
[CPM](https://github.com/cpm-cmake). Here is a minimal working example:

```cmake
cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(MyProject)

# Download and include CPM
file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/get_cpm.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

# Import libsg package
CPMAddPackage(
  NAME libsg
  GITHUB_REPOSITORY samangh/libsg
  GIT_TAG master
  GIT_SHALLOW
  GIT_SUBMODULES_RECURSE ON
  OPTIONS
  "LIBSG_BUILD_TESTING OFF"
  "LIBSG_IMGUI OFF")

# Add as dependency
add_executable(main main.cpp)
target_link_libraries(main PUBLIC SG::common)
```

## Code Examples
### TCP server

Here is an echo server:

```cpp
#include <iostream>
#include <sg/tcp_server.h>

using namespace sg::net;

int main(int, char**) {
    tcp_server server;

    tcp_server::session_data_available_cb_t onDataReceived =
        [](tcp_server& l, auto id, const auto* data, size_t size) {
            auto w = sg::make_shared_c_buffer<std::byte>(size);
            std::memcpy(w.get(), data, size);
            l.write(id, w);
        };

    sg::net::end_point ep("127.0.0.1", 55555);

    server.start({ep}, nullptr, nullptr, nullptr, onDataReceived, nullptr);
    // Wait until server is stopped
    server.future_get_once();
}
```

You can also add callbacks for connection/disconnection events, for example:

```cpp
#include <iostream>
#include <sg/tcp_server.h>

using namespace sg::net;

int main(int, char**) {
    tcp_server server;

    tcp_server::session_created_cb_t onClientConnected = [&](tcp_server&, auto) {
        std::cout << "Client connected" << std::endl;
    };

    tcp_server::session_disconnected_cb_t onClientDisconnected =
        [&](tcp_server&, auto, auto) { std::cout << "Client disconnected" << std::endl; };

    tcp_server::session_data_available_cb_t onDataReceived =
        [](tcp_server& l, auto id, const auto* data, size_t size) {
            auto w = sg::make_shared_c_buffer<std::byte>(size);
            std::memcpy(w.get(), data, size);
            l.write(id, w);
        };

    sg::net::end_point ep("127.0.0.1", 55555);

    server.start({ep}, nullptr, nullptr, nullptr, onDataReceived, nullptr);
    // Wait until server is stopped
    server.future_get_once();
}
```
