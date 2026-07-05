# Plan: `net_http` Echo Server + libcurl Client Example

An example application, built with **CMake**, that demonstrates the
[`google/net_http`](https://github.com/google/net_http) lightweight C++ HTTP
library. It consists of:

- **`echo_server`** — a `net_http` HTTP server that echoes the incoming
  request's HTTP headers and (for `POST`) the request text body back in the
  response body as `text/plain`.
- **`echo_client`** — an interactive command-line HTTP client, built on
  **libcurl**, that reads request lines from stdin in a loop (an empty line
  sends the accumulated body as a `POST`, Ctrl-D quits) and prints each echoed
  response.

All artifacts live under `examples/net_http/` in the `wenbozhu2011/examples`
repository.

---

## 1. Goals & scope

| Requirement | How it is met |
| --- | --- |
| Use the `net_http` library | Server is built directly on `net_http::HTTPServerInterface` / `ServerRequestInterface`. |
| Depend on **`google/net_http`** (not the fork) | Pulled via CMake `FetchContent` from `https://github.com/google/net_http`, pinned to commit `0381f0c`. |
| Echo request **headers** and **POST text body** in the response body | `EchoHandler` walks `request_headers()` and drains `ReadRequestBytes()`, writing them to the response as `text/plain`. |
| HTTP **client uses libcurl** | `echo_client` uses `libcurl`'s easy interface (`CURLOPT_POSTFIELDS`, `CURLOPT_HTTPHEADER`, `CURLOPT_WRITEFUNCTION`). |
| Build with **CMake** (not Bazel) | Top-level `CMakeLists.txt`; a `net_http` static library target is compiled from the upstream sources by our own CMake glue (upstream ships only Bazel `BUILD` files). |
| Other deps: **libcurl** and **libevent** | libcurl → client; libevent → the `net_http` server backend (`evhttp`). Plus Abseil and zlib, which `net_http` requires transitively. |
| Follow the `net_http` C++ code convention | Google C++ style, Apache-2.0 license header, `namespace`-scoped `using` declarations, `net_http` namespace, C++14-compatible source. |

Non-goals: TLS/HTTPS, HTTP/2, streaming bodies, gzip request handling,
production hardening. The server is single-purpose (an echo endpoint).

---

## 2. Directory layout

```
examples/net_http/
├── PLAN.md                     # this document
├── README.md                   # build + run instructions
├── CMakeLists.txt              # top-level project, dependency wiring
├── cmake/
│   └── BuildNetHttp.cmake      # FetchContent google/net_http, define net_http_server static lib
├── server/
│   ├── CMakeLists.txt
│   └── echo_server.cc          # net_http echo server
└── client/
    ├── CMakeLists.txt
    └── echo_client.cc          # libcurl echo client
```

---

## 3. Dependencies & how each is obtained

`net_http` is a Bazel project and ships **no** CMake build files, so the core
of this plan is a small CMake shim that compiles the handful of upstream
`.cc` files we need into a static library. The dependency graph below is
derived directly from the upstream `BUILD` files.

| Dependency | Role | Acquisition (CMake) |
| --- | --- | --- |
| **google/net_http** `@0381f0c` | HTTP server library | `FetchContent_Declare(GIT_REPOSITORY https://github.com/google/net_http GIT_TAG 0381f0c286244441a47b08febe3728a68698b954)` — source only, compiled by `cmake/BuildNetHttp.cmake`. |
| **libevent** (`>= 2.1.12`) | `net_http` server backend (`evhttp`) | System package (`libevent-dev`) via `pkg-config`: modules `libevent`, `libevent_pthreads`. FetchContent fallback from `libevent/libevent`. |
| **Abseil** | `net_http` uses `absl::strings`, `memory`, `synchronization`, `base`, `types/span`, `time` | `FetchContent` from `abseil/abseil-cpp`, tag `20240722.0`; or system `find_package(absl)`. |
| **zlib** | `net_http/compression/gzip_zlib.cc` | `find_package(ZLIB REQUIRED)`. |
| **libcurl** | Echo **client** transport | `find_package(CURL REQUIRED)` → target `CURL::libcurl`. |
| Threads | libevent pthreads + `net_http` executor | `find_package(Threads REQUIRED)` → `Threads::Threads`. |

### 3.1 `net_http` sources we must compile

From the upstream `BUILD` graph (`//net_http/server/public:http_server` →
`evhttp_server` → `gzip_zlib`, `net_logging`, `shared_files`), the static
library `net_http_server` compiles exactly these translation units from the
fetched source tree (root = the directory that contains `net_http/`):

```
net_http/public/header_names.cc
net_http/internal/net_logging.cc
net_http/compression/gzip_zlib.cc
net_http/server/internal/evhttp_request.cc
net_http/server/internal/evhttp_server.cc
```

Public headers consumed by the example (`httpserver.h`,
`httpserver_interface.h`, `server_request_interface.h`,
`response_code_enum.h`) are header-only. Includes in these sources are rooted
at the repo top (e.g. `#include "net_http/server/public/httpserver.h"`), so the
fetched repo root is added as an **include directory**.

External includes pulled by these sources (verified in-tree):
`event2/{buffer,event,http,keyvalq_struct,thread,util}.h` (libevent core +
extra + pthreads), `zlib.h`, and `absl/...`.

### 3.2 `cmake/BuildNetHttp.cmake` (shim outline)

```cmake
include(FetchContent)
FetchContent_Declare(net_http
  GIT_REPOSITORY https://github.com/google/net_http.git
  GIT_TAG        0381f0c286244441a47b08febe3728a68698b954)
FetchContent_MakeAvailable(net_http)          # populate only; no upstream CMake

add_library(net_http_server STATIC
  ${net_http_SOURCE_DIR}/net_http/public/header_names.cc
  ${net_http_SOURCE_DIR}/net_http/internal/net_logging.cc
  ${net_http_SOURCE_DIR}/net_http/compression/gzip_zlib.cc
  ${net_http_SOURCE_DIR}/net_http/server/internal/evhttp_request.cc
  ${net_http_SOURCE_DIR}/net_http/server/internal/evhttp_server.cc)

target_include_directories(net_http_server PUBLIC ${net_http_SOURCE_DIR})
target_link_libraries(net_http_server PUBLIC
  absl::strings absl::memory absl::synchronization absl::base
  absl::span absl::time absl::core_headers
  ZLIB::ZLIB PkgConfig::LIBEVENT Threads::Threads)
target_compile_features(net_http_server PUBLIC cxx_std_17)
```

> **C++ standard note:** upstream builds at `-std=c++14`. The sources are
> forward-compatible, so we compile the whole example (and the vendored
> `net_http` sources) at **C++17** to match modern Abseil releases. This is the
> single most likely friction point; if a pinned Abseil that still supports
> C++14 is preferred, that is a drop-in swap.

---

## 4. Server design — `server/echo_server.cc`

Modeled on the upstream `net_http/server/testing/evhttp_echo_server.cc`, but
responding as **`text/plain`** and echoing the **POST body**.

- **Endpoint:** `POST /echo` (also accepts other methods; body echo applies to
  whatever body is present).
- **Executor:** a `MyExecutor : public net_http::EventExecutor`. For a genuinely
  concurrent server we back it with the upstream `FixedThreadPool`
  (`net_http/internal/fixed_thread_pool.h`); a run-inline executor is the
  minimal alternative. (Chosen: `FixedThreadPool(4)`.)
- **Handler `EchoHandler(ServerRequestInterface* req)`** builds a plain-text
  body:
  1. Request line: `req->http_method()` + `req->uri_path()`.
  2. A `Headers:` section iterating `req->request_headers()` and
     `req->GetRequestHeader(name)`.
  3. A `Body:` section: loop `req->ReadRequestBytes(&n)` until `nullptr`,
     appending each chunk.
  4. `req->WriteResponseString(response)`, `SetContentTypeTEXT(req)`,
     `req->ReplyWithStatus(HTTPStatusCode::OK)`.
- **Bootstrap `StartServer(port)`:** `ServerOptions` → `AddPort` +
  `SetExecutor` → `CreateEvHTTPServer` → `RegisterRequestHandler("/echo", …)`
  → `StartAcceptingRequests()`.
- **`main(argc, argv)`:** parse port with `absl::SimpleAtoi`, then
  `server->WaitForTermination()`.

API surface used (all public `net_http`): `HTTPServerInterface`,
`ServerOptions`, `RequestHandlerOptions`, `EventExecutor`,
`ServerRequestInterface`, `CreateEvHTTPServer`, `SetContentTypeTEXT`,
`HTTPStatusCode`.

Response body shape:

```
POST /echo

Headers:
Host: 127.0.0.1:8080
Content-Type: text/plain
X-Example: demo
Content-Length: 27

Body:
hello from the libcurl client
```

---

## 5. Client design — `client/echo_client.cc`

A libcurl easy-interface program that runs an **interactive read/send loop**
against the server.

- **Usage:** `echo_client <url>` — the URL is `argv[1]`; request bodies are read
  from **stdin** line by line, and each response body is written to **stdout**.
  - Interactive: `echo_client http://127.0.0.1:8080/echo`, then type lines and
    press Enter on an empty line to send.
  - Piped: `printf 'hello\n\n' | echo_client http://127.0.0.1:8080/echo` (the
    trailing blank line triggers the send).
- **Interactive loop (`std::getline`):**
  - Each **non-empty line** is appended to the current request body (with a
    trailing `\n`).
  - An **empty line** sends the accumulated body as a `POST`, prints the
    response, then clears the buffer for the next request.
  - **Ctrl-D / EOF** closes the connection and terminates the client. A partial
    (un-sent) buffer is not flushed — the empty line is the send trigger.
  - When stdin is a TTY (`isatty`), a short usage banner and a `> ` prompt are
    printed to stderr; piped input runs the same loop without the prompt.
- **libcurl setup (one reused handle → HTTP keep-alive for the session):**
  - `curl_easy_init()` once; set `CURLOPT_URL`, `CURLOPT_POST`,
    `CURLOPT_HTTPHEADER` (demo headers `Content-Type: text/plain`,
    `X-Example: demo` via `curl_slist_append`), and `CURLOPT_WRITEFUNCTION` up
    front so the connection is reused across requests.
  - Per request: set `CURLOPT_POSTFIELDS` / `CURLOPT_POSTFIELDSIZE` to the
    buffer (explicit size handles binary/embedded-NUL bodies) and
    `CURLOPT_WRITEDATA` to a fresh response `std::string`, then
    `curl_easy_perform`, check `CURLcode`, and read `CURLINFO_RESPONSE_CODE` and
    `CURLINFO_TOTAL_TIME_T` (round-trip time in **microseconds**).
- **Output:** writes each echoed **response body to stdout** (nothing else on
  stdout, so it can be piped/compared); logs the HTTP **status code and the
  request latency in microseconds** — e.g. `HTTP 200 (1234 us)` — plus any
  transport error, to **stderr**. Cleans up with `curl_slist_free_all` /
  `curl_easy_cleanup`, wrapped in `curl_global_init` / `curl_global_cleanup`
  around `main`. The exit code reflects the **last** request (non-zero on
  transport failure or a non-2xx status).
- **Convention:** same Apache header + `namespace {}`-local helpers as the
  server; a `WriteCallback` free function for the write callback.

---

## 6. CMake wiring

- **Top-level `CMakeLists.txt`:** `cmake_minimum_required(VERSION 3.19)`,
  `project(net_http_example CXX)`, `CMAKE_CXX_STANDARD 17`. Finds
  `PkgConfig`+libevent, `ZLIB`, `CURL`, `Threads`; makes Abseil available;
  `include(cmake/BuildNetHttp.cmake)`; `add_subdirectory(server)` and
  `add_subdirectory(client)`.
- **`server/CMakeLists.txt`:** `add_executable(echo_server echo_server.cc)` →
  link `net_http_server` (transitively brings libevent/zlib/absl).
- **`client/CMakeLists.txt`:** `add_executable(echo_client echo_client.cc)` →
  link `CURL::libcurl`.

---

## 7. Build & run

```bash
# 1. Prerequisites (Debian/Ubuntu). `git` is required both to clone this
#    repository and for CMake FetchContent to pull google/net_http at configure
#    time; the rest are the build toolchain and the C/C++ library dependencies.
sudo apt-get update
sudo apt-get install -y git cmake g++ pkg-config \
    libevent-dev zlib1g-dev libcurl4-openssl-dev
#    (macOS/Homebrew equivalent: `brew install git cmake pkg-config libevent curl`.)

# 2. Download the source from the repo and enter the example directory.
git clone https://github.com/wenbozhu2011/examples.git
cd examples/examples/net_http

# 3. Configure and build. The first `cmake` run uses git to fetch
#    google/net_http (pinned @0381f0c) and Abseil via FetchContent.
cmake -S . -B build
cmake --build build -j

# 4. Terminal 1: start the server on port 8080.
./build/server/echo_server 8080

# 5. Terminal 2: run the interactive client. Type one or more lines, then press
#    Enter on an empty line to send the request; Ctrl-D quits.
./build/client/echo_client http://127.0.0.1:8080/echo
#    Or drive it non-interactively (the trailing blank line triggers the send):
printf 'hello from the libcurl client\n\n' | ./build/client/echo_client http://127.0.0.1:8080/echo
```

Expected: after the empty line, the client prints the plain-text echo to
**stdout** — the request line, the headers it sent (`X-Example: demo`,
`Content-Type`, `Host`, `Content-Length`), and the body text — while the
`HTTP 200` status and the round-trip latency in microseconds (e.g.
`HTTP 200 (1234 us)`) are logged to **stderr**. The loop then waits for the next
request until Ctrl-D.

---

## 8. Verification

1. **Build** succeeds (`echo_server`, `echo_client`).
2. **Interactive round-trip:** `printf 'hi\n\n' | echo_client <url>` — stderr
   logs `HTTP 200 (<n> us)` (status plus microsecond latency); stdout contains
   the sent `X-Example: demo` header and the exact POST text (`hi`).
3. **Multiple requests in one session / keep-alive:**
   `printf 'one\n\ntwo\n\n' | echo_client <url>` sends two requests over the same
   reused connection and prints two echoed responses.
4. **Cross-check with `curl`:**
   `curl -s -X POST -H 'X-Example: demo' --data 'hi' http://127.0.0.1:8080/echo`
   returns the same echo.
5. **Empty request:** an empty line with no accumulated body (`printf '\n'`)
   sends an empty `POST` — the server returns headers with an empty `Body:`
   section, no crash on `ReadRequestBytes` returning `nullptr` immediately.
6. **Clean exit:** Ctrl-D / EOF terminates the client with no error.

---

## 9. Coding conventions (matching `net_http`)

- Apache-2.0 license header block at the top of every `.cc` file.
- Google C++ style (2-space indent, `//` comments, no `using namespace`).
- File-local declarations in an anonymous `namespace {}` with explicit
  `using net_http::Foo;` aliases, mirroring `evhttp_echo_server.cc`.
- Prefer `absl` utilities already used by `net_http` (`absl::StrAppend`,
  `absl::SimpleAtoi`, `absl::make_unique`) for consistency.
- C++14-compatible source (compiled at C++17).

---

## 10. Risks & open questions

- **C++ standard vs. Abseil:** modern Abseil LTS requires C++17; upstream
  `net_http` is C++14. Resolved by compiling everything at C++17 (§3.2). If a
  strict C++14 build is desired, pin Abseil `20240116.x`.
- **libevent discovery:** `pkg-config` is the primary path (`libevent-dev`);
  the `FetchContent` fallback (building libevent from source) is available if
  no system package exists. `evhttp` lives in libevent's *extra* lib, and
  `evthread_use_pthreads` needs *pthreads* — both are covered by linking the
  `libevent` + `libevent_pthreads` pkg-config modules.
- **Vendored source list drift:** if a future `net_http` commit adds/removes
  server `.cc` files, `cmake/BuildNetHttp.cmake` must be updated. Pinning to
  `0381f0c` keeps the list stable.
- **Header-only executor reuse:** `fixed_thread_pool.h` is marked `testonly` in
  Bazel but is a plain header; using it here is convenient and dependency-free.
  A local thread pool is a trivial alternative if we prefer not to reach into
  an upstream internal header.

---

## 11. Deliverables checklist

- [ ] `cmake/BuildNetHttp.cmake` — FetchContent + `net_http_server` static lib
- [ ] `CMakeLists.txt` — top-level dependency wiring
- [ ] `server/echo_server.cc` + `server/CMakeLists.txt`
- [ ] `client/echo_client.cc` + `client/CMakeLists.txt`
- [ ] `README.md` — build/run instructions
- [ ] Manual verification per §8
