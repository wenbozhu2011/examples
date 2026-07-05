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

// A libcurl-based client for the net_http echo server.
//
// Reads the POST request body from stdin, writes the server's response body to
// stdout, and logs the HTTP status code and round-trip latency (microseconds)
// to stderr. Keeping stdout limited to the response body lets the client be
// piped or diffed. Usage:
//   echo 'hello' | echo_client http://127.0.0.1:8080/echo
//   echo_client http://127.0.0.1:8080/echo < message.txt

#include <curl/curl.h>

#include <cstddef>
#include <iostream>
#include <iterator>
#include <string>

namespace {

// libcurl write callback: appends received bytes to the target std::string.
size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  const size_t num_bytes = size * nmemb;
  static_cast<std::string*>(userdata)->append(ptr, num_bytes);
  return num_bytes;
}

// POSTs `body` to `url`, printing the response to stdout and the status and
// latency to stderr. Returns 0 on a 2xx response, 1 otherwise.
int SendRequest(const char* url, const std::string& body) {
  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    std::cerr << "Failed to initialize libcurl." << std::endl;
    return 1;
  }

  std::string response;
  curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: text/plain");
  headers = curl_slist_append(headers, "X-Example: demo");

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  const CURLcode code = curl_easy_perform(curl);

  int exit_code = 0;
  if (code != CURLE_OK) {
    std::cerr << "Request failed: " << curl_easy_strerror(code) << std::endl;
    exit_code = 1;
  } else {
    long status = 0;
    curl_off_t latency_us = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T, &latency_us);

    std::cout << response;
    std::cerr << "HTTP " << status << " (" << latency_us << " us)" << std::endl;

    if (status < 200 || status >= 300) {
      exit_code = 1;
    }
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return exit_code;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: echo_client <url>   (POST body is read from stdin)"
              << std::endl;
    return 1;
  }

  // Read the entire request body from stdin (binary-safe).
  const std::string body{std::istreambuf_iterator<char>(std::cin),
                         std::istreambuf_iterator<char>()};

  curl_global_init(CURL_GLOBAL_DEFAULT);
  const int exit_code = SendRequest(argv[1], body);
  curl_global_cleanup();
  return exit_code;
}
