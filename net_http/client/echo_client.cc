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

// An interactive libcurl-based client for the net_http echo server.
//
// The client reads request lines from stdin in a loop:
//   - each non-empty line is appended to the current request body;
//   - an empty line sends the accumulated body as a POST to the server;
//   - Ctrl-D (EOF) closes the connection and terminates the client.
//
// Each response body is written to stdout wrapped in [] (so its exact
// boundaries are visible), and the HTTP status code and round-trip latency
// (microseconds) are logged to stderr, so stdout stays limited to server
// responses. A single libcurl handle is reused for the whole session, so the
// underlying HTTP connection is kept alive across requests.
//
// Usage:
//   echo_client http://127.0.0.1:8080/echo
//   printf 'hello\n\n' | echo_client http://127.0.0.1:8080/echo

#include <curl/curl.h>
#include <unistd.h>

#include <cstddef>
#include <iostream>
#include <string>

namespace {

// libcurl write callback: appends received bytes to the target std::string.
size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  const size_t num_bytes = size * nmemb;
  static_cast<std::string*>(userdata)->append(ptr, num_bytes);
  return num_bytes;
}

// POSTs `body` over the reused `curl` handle, printing the response to stdout
// (wrapped in []) and the status and latency to stderr. Returns 0 on a 2xx
// response, 1 otherwise.
int SendRequest(CURL* curl, const std::string& body) {
  std::string response;
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  const CURLcode code = curl_easy_perform(curl);
  if (code != CURLE_OK) {
    std::cerr << "Request failed: " << curl_easy_strerror(code) << std::endl;
    return 1;
  }

  long status = 0;
  curl_off_t latency_us = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T, &latency_us);

  // Wrap the response body in [] so its exact boundaries are visible on stdout.
  std::cout << '[' << response << ']' << std::endl;
  std::cerr << "HTTP " << status << " (" << latency_us << " us)" << std::endl;

  return (status >= 200 && status < 300) ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: echo_client <url>   (reads request lines from stdin)"
              << std::endl;
    return 1;
  }
  const char* url = argv[1];

  curl_global_init(CURL_GLOBAL_DEFAULT);

  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    std::cerr << "Failed to initialize libcurl." << std::endl;
    curl_global_cleanup();
    return 1;
  }

  // A single reused handle keeps the HTTP connection alive across requests for
  // the whole interactive session.
  curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: text/plain");
  headers = curl_slist_append(headers, "X-Example: demo");

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

  const bool interactive = isatty(STDIN_FILENO) != 0;
  if (interactive) {
    std::cerr << "Interactive echo client for " << url << "\n"
              << "  - type one or more lines, then an empty line to send\n"
              << "  - press Ctrl-D to quit" << std::endl;
  }

  int last_status = 0;
  std::string body;
  std::string line;
  while (true) {
    if (interactive) {
      std::cerr << "> ";
    }
    if (!std::getline(std::cin, line)) {
      break;  // Ctrl-D / EOF: close the connection and terminate.
    }
    if (line.empty()) {
      last_status = SendRequest(curl, body);
      body.clear();
      continue;
    }
    body.append(line);
    body.push_back('\n');
  }
  if (interactive) {
    std::cerr << std::endl;  // finish the prompt line after Ctrl-D
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return last_status;
}
