add_executable(echo_client echo_client.cc)

target_link_libraries(echo_client PRIVATE CURL::libcurl)
