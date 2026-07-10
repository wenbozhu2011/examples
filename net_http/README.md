# net_http echo server + libcurl client

A small example that demonstrates the
[`google/net_http`](https://github.com/google/net_http) lightweight C++ HTTP
library, built with **CMake**:

- **`echo_server`** — a `net_http` HTTP server. `POST /echo` returns the request
  method, URI, request headers, and request body as a `text/plain` response. A
  server-side **interceptor** (a pre-hook registered via net_http's interceptor
  API) logs each request's headers to the server's **stdout** before the handler
  runs.
- **`echo_client`** — an interactive **libcurl** client. It reads request lines
  from **stdin** in a loop (an empty line sends the accumulated body as a POST,
  Ctrl-D quits), writes each response to **stdout**, and logs the HTTP status
  code and round-trip latency (microseconds) to **stderr**.

See [`PLAN.md`](PLAN.md) for the full design and rationale.

## Dependencies

Fetched automatically by CMake (`FetchContent`, needs network on first configure):

- **net_http**, pinned to the tip of the
  [`server_interceptor` branch of `wenbozhu2011/net_http`](https://github.com/wenbozhu2011/net_http/tree/server_interceptor)
  (commit `c56de14`). This example uses net_http's server-side **interceptor
  API**, which is not yet on upstream
  [`google/net_http`](https://github.com/google/net_http), so the dependency
  points at the fork's branch; the compiled source list is otherwise identical to
  upstream. Since net_http ships only Bazel build files,
  [`cmake/BuildNetHttp.cmake`](cmake/BuildNetHttp.cmake) compiles the required
  sources into a `net_http_server` static library.
- [Abseil](https://github.com/abseil/abseil-cpp) (tag `20240722.0`).

Provided by the system (install these yourself):

- **libcurl** — the client transport.
- **libevent** (`>= 2.1.12`) — the `net_http` server backend (`evhttp`).
- **zlib** — required transitively by `net_http`.
- A C++17 compiler, CMake `>= 3.19`, `git`, and `pkg-config`.

## Build

```bash
# Prerequisites (Debian/Ubuntu). git is needed both to clone this repo and for
# CMake FetchContent to pull net_http and Abseil at configure time.
sudo apt-get update
sudo apt-get install -y git cmake g++ pkg-config \
    libevent-dev zlib1g-dev libcurl4-openssl-dev
# macOS/Homebrew equivalent:
#   brew install git cmake pkg-config libevent curl

# Download the source and enter this example directory.
git clone https://github.com/wenbozhu2011/examples.git
cd examples/net_http

# Configure: the first run fetches net_http and Abseil via FetchContent,
# then generates the build system under ./build.
cmake -S . -B build
# Compile the net_http library, echo_server, and echo_client (-j builds in
# parallel).
cmake --build build -j
```

## Run

```bash
# Terminal 1: start the server on port 8080.
./build/server/echo_server 8080

# Terminal 2: run the interactive client. Type one or more lines, then press
# Enter on an empty line to send the request; Ctrl-D quits.
./build/client/echo_client http://127.0.0.1:8080/echo

# Or drive it non-interactively — the trailing blank line triggers the send:
printf 'hello from the libcurl client\n\n' | ./build/client/echo_client http://127.0.0.1:8080/echo
```

Expected: after the empty line, the client prints the plain-text echo to stdout,
wrapped in `[]` — the request line, the headers it sent (`X-Example: demo`,
`Content-Type`, `Host`, `Content-Length`), and the body text — while the status
and latency are logged to stderr, e.g. `HTTP 200 (1234 us)`. The loop then waits
for the next request until Ctrl-D.

On the **server** side, the interceptor prints each request's headers to the
server's stdout before the response is produced:

```
[interceptor] POST /echo
[interceptor]   Host: 127.0.0.1:8080
[interceptor]   Accept: */*
[interceptor]   Content-Type: text/plain
[interceptor]   X-Example: demo
[interceptor]   Content-Length: 22
```

You can cross-check the server with plain `curl`:

```bash
curl -s -X POST -H 'X-Example: demo' --data 'hi' http://127.0.0.1:8080/echo
```
