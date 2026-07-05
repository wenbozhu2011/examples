# Example: a net_http echo server and a libcurl echo client.
#
# Build:
#   cmake -S . -B build
#   cmake --build build -j
#
# The first configure step uses git (via FetchContent) to pull google/net_http
# and Abseil, so a network connection is required the first time.

cmake_minimum_required(VERSION 3.19)
project(net_http_example CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

# libevent: the net_http server backend (evhttp). evhttp lives in libevent's
# "extra" library and evthread_use_pthreads() needs the pthreads integration,
# so we request both modules as one imported target (PkgConfig::LIBEVENT).
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED IMPORTED_TARGET libevent libevent_pthreads)

# zlib: required by net_http/compression/gzip_zlib.cc.
find_package(ZLIB REQUIRED)

# libcurl: the echo client's HTTP transport.
find_package(CURL REQUIRED)

# Threads: pthreads for libevent and the server's thread-pool executor.
find_package(Threads REQUIRED)

# Abseil: the utility library net_http (and this example) links against.
include(FetchContent)
set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_ENABLE_INSTALL OFF)
FetchContent_Declare(
  absl
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG        20240722.0)
FetchContent_MakeAvailable(absl)

# Compile google/net_http into a static library target (net_http_server).
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildNetHttp.cmake)

# ---------------------------------------------------------------------------
# Example targets
# ---------------------------------------------------------------------------

add_subdirectory(server)
add_subdirectory(client)
