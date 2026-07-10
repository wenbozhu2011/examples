/* Copyright 2026 The net_http Example Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// A multi-threaded HTTP server that echoes the request headers and the request
// body back to the client as text/plain.
// URI: /echo
//
// A server-side pre-hook interceptor (net_http's interceptor API) logs each
// request's headers to stdout before the handler runs.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

#include "net_http/internal/fixed_thread_pool.h"
#include "net_http/public/response_code_enum.h"
#include "net_http/server/public/httpserver.h"
#include "net_http/server/public/httpserver_interface.h"
#include "net_http/server/public/server_request_interface.h"

namespace {

using absl::StrAppend;

using net_http::EventExecutor;
using net_http::FixedThreadPool;
using net_http::HTTPServerInterface;
using net_http::HTTPStatusCode;
using net_http::InterceptResult;
using net_http::RequestHandlerOptions;
using net_http::ServerOptions;
using net_http::ServerRequestInterface;
using net_http::SetContentTypeTEXT;

// An executor that runs the server's I/O and handler callbacks on a fixed-size
// thread pool, so requests can be served concurrently.
class ThreadPoolExecutor final : public EventExecutor {
 public:
  explicit ThreadPoolExecutor(int num_threads) : thread_pool_(num_threads) {}

  void Schedule(std::function<void()> fn) override {
    thread_pool_.Schedule(std::move(fn));
  }

 private:
  FixedThreadPool thread_pool_;
};

// Pre-hook interceptor: logs the request line and every request header to
// stdout, then returns kContinue so the request handler still runs. Registered
// per URI via RegisterRequestInterceptor (see StartServer).
InterceptResult LogRequestHeaders(ServerRequestInterface* req) {
  std::cout << "[interceptor] " << req->http_method() << " " << req->uri_path()
            << "\n";
  for (absl::string_view header : req->request_headers()) {
    std::cout << "[interceptor]   " << header << ": "
              << req->GetRequestHeader(header) << "\n";
  }
  std::cout << std::flush;
  return InterceptResult::kContinue;
}

// Echoes the request line, the request headers, and the request body back to
// the client as a plain-text response.
void EchoHandler(ServerRequestInterface* req) {
  std::string response;

  StrAppend(&response, req->http_method(), " ", req->uri_path(), "\n\n");

  StrAppend(&response, "Headers:\n");
  for (absl::string_view header : req->request_headers()) {
    StrAppend(&response, header, ": ", req->GetRequestHeader(header), "\n");
  }

  StrAppend(&response, "\nBody:\n");
  int64_t num_bytes = 0;
  auto request_chunk = req->ReadRequestBytes(&num_bytes);
  while (request_chunk != nullptr) {
    StrAppend(&response, absl::string_view(request_chunk.get(),
                                           static_cast<size_t>(num_bytes)));
    request_chunk = req->ReadRequestBytes(&num_bytes);
  }

  req->WriteResponseString(response);
  SetContentTypeTEXT(req);
  req->ReplyWithStatus(HTTPStatusCode::OK);
}

// Returns the server if successful, or nullptr if there is any error.
std::unique_ptr<HTTPServerInterface> StartServer(int port, int num_threads) {
  auto options = absl::make_unique<ServerOptions>();
  options->AddPort(port);
  options->SetExecutor(absl::make_unique<ThreadPoolExecutor>(num_threads));

  auto server = CreateEvHTTPServer(std::move(options));
  if (server == nullptr) {
    return nullptr;
  }

  RequestHandlerOptions handler_options;
  server->RegisterRequestHandler("/echo", EchoHandler, handler_options);

  // Pre-hook interceptor that logs each /echo request's headers to stdout. The
  // post-hook is nullptr (pre-only). A RegisterRequestInterceptorDispatcher
  // could apply the same hook to every route instead of a single URI.
  server->RegisterRequestInterceptor("/echo", LogRequestHeaders,
                                     /*response_interceptor=*/nullptr);

  if (!server->StartAcceptingRequests()) {
    return nullptr;
  }
  return server;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: echo_server <port>" << std::endl;
    return 1;
  }

  int port = 0;
  if (!absl::SimpleAtoi(argv[1], &port)) {
    std::cerr << "Invalid port: " << argv[1] << std::endl;
    return 1;
  }

  auto server = StartServer(port, /*num_threads=*/4);
  if (server == nullptr) {
    std::cerr << "Failed to start the server on port " << port << std::endl;
    return 1;
  }

  std::cerr << "Echo server listening on port " << port << " (POST to /echo)"
            << std::endl;
  server->WaitForTermination();
  return 0;
}
