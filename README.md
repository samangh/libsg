[![Windows](https://github.com/samangh/libsg/actions/workflows/windows.yml/badge.svg)](https://github.com/samangh/libsg/actions/workflows/windows.yml)
[![MSYS2](https://github.com/samangh/libsg/actions/workflows/msys2.yml/badge.svg)](https://github.com/samangh/libsg/actions/workflows/msys2.yml)
[![Linux](https://github.com/samangh/libsg/actions/workflows/linux.yml/badge.svg)](https://github.com/samangh/libsg/actions/workflows/linux.yml)
[![macOS](https://github.com/samangh/libsg/actions/workflows/macos.yml/badge.svg)](https://github.com/samangh/libsg/actions/workflows/macos.yml)
[![codecov](https://codecov.io/gh/samangh/libsg/graph/badge.svg?token=ZAL8QI6GQR)](https://codecov.io/gh/samangh/libsg)

## How to use

First, include [CPM](https://github.com/cpm-cmake) in your project if
you haven't already. For example,

``` cmake
file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/get_cpm.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)
```

Then include using:

``` cmake
CPMAddPackage(
  NAME libsg
  GITHUB_REPOSITORY samangh/libsg
  GIT_TAG a6382883e4f37bd8a297a93677a130f6f362892c
  GIT_SHALLOW
  GIT_SUBMODULES_RECURSE ON
  OPTIONS
  "LIBSG_BUILD_TESTING OFF"
  "LIBSG_IMGUI OFF"
)
```

And add `SG::common` as a dependency to your target, for example:

```cmake
target_link_libraries(main PUBLIC SG::common)
```

So, combined together here is a minimal working example:

```cmake
cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# create project
project(MyProject)

file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/get_cpm.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

CPMAddPackage(
  NAME libsg
  GITHUB_REPOSITORY samangh/libsg
  GIT_TAG a6382883e4f37bd8a297a93677a130f6f362892c
  GIT_SHALLOW
  GIT_SUBMODULES_RECURSE ON
  OPTIONS
  "LIBSG_BUILD_TESTING OFF"
  "LIBSG_IMGUI OFF"
)

add_executable(main main.cpp)
target_link_libraries(main PUBLIC SG::common)
```

## Examples
### TCP server

Here is an echo server:

```cpp
#include <iostream>
#include <sg/tcp_server.h>

using namespace sg::net;

int main(int, char**) {
    tcp_server server;

    tcp_server::session_data_available_cb_t onDataReceived =
        [](tcp_server& l, tcp_server::session_id_t id, const std::byte* dat, size_t size) {
            auto w = sg::make_shared_c_buffer<std::byte>(size);
            std::memcpy(w.get(), dat, size);
            l.write(id, w);
        };

    sg::net::end_point ep("127.0.0.1", 55555);

    server.start({ep}, nullptr, nullptr, nullptr, onDataReceived, nullptr);
    // Wait until server is stopped
    server.future_get_once();
}
```

You can also add callbacks for connection/disconnection events, for exammple:

```cpp
#include <iostream>
#include <sg/tcp_server.h>

using namespace sg::net;

int main(int, char**) {
    tcp_server server;

    tcp_server::session_created_cb_t onClientConnected = [&](tcp_server&,
                                                             sg::net::tcp_server::session_id_t) {
        std::cout << "Client connected" << std::endl;
    };


    tcp_server::session_disconnected_cb_t onClientDisconnected =
        [&](tcp_server&, tcp_server::session_id_t, std::optional<std::exception>) {
            std::cout << "Client disconnected" << std::endl;
        };
    
    tcp_server::session_data_available_cb_t onDataReceived =
        [](tcp_server& l, tcp_server::session_id_t id, const std::byte* dat, size_t size) {
            auto w = sg::make_shared_c_buffer<std::byte>(size);
            std::memcpy(w.get(), dat, size);
            l.write(id, w);
        };

    sg::net::end_point ep("127.0.0.1", 55555);

    server.start({ep}, nullptr, nullptr, onClientConnected, onDataReceived, onClientDisconnected);

    // Wait until server is stopped
    server.future_get_once();
}
```
