# net_http echo server + libcurl client

A small example that demonstrates the
[`google/net_http`](https://github.com/google/net_http) lightweight C++ HTTP
library, built with **CMake**:

- **`echo_server`** — a `net_http` HTTP server. `POST /echo` returns the request
  method, URI, request headers, and request body as a `text/plain` response.
- **`echo_client`** — an interactive **libcurl** client. It reads request lines
  from **stdin** in a loop (an empty line sends the accumulated body as a POST,
  Ctrl-D quits), writes each response to **stdout**, and logs the HTTP status
  code and round-trip latency (microseconds) to **stderr**.

See [`PLAN.md`](PLAN.md) for the full design and rationale.

## Dependencies

Fetched automatically by CMake (`FetchContent`, needs network on first configure):

- [`google/net_http`](https://github.com/google/net_http), pinned to commit
  `0381f0c`. Since upstream ships only Bazel build files,
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
# CMake FetchContent to pull google/net_http and Abseil at configure time.
sudo apt-get update
sudo apt-get install -y git cmake g++ pkg-config \
    libevent-dev zlib1g-dev libcurl4-openssl-dev
# macOS/Homebrew equivalent:
#   brew install git cmake pkg-config libevent curl

# Download the source and enter this example directory.
git clone https://github.com/wenbozhu2011/examples.git
cd examples/examples/net_http

# Configure and build.
cmake -S . -B build
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

Expected: after the empty line, the client prints the plain-text echo to stdout
— the request line, the headers it sent (`X-Example: demo`, `Content-Type`,
`Host`, `Content-Length`), and the body text — while the status and latency are
logged to stderr, e.g. `HTTP 200 (1234 us)`. The loop then waits for the next
request until Ctrl-D.

You can cross-check the server with plain `curl`:

```bash
curl -s -X POST -H 'X-Example: demo' --data 'hi' http://127.0.0.1:8080/echo
```
