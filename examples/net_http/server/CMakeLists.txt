add_executable(echo_server echo_server.cc)

# net_http_server publicly carries its include directory and the Abseil /
# libevent / zlib / pthreads link dependencies.
target_link_libraries(echo_server PRIVATE net_http_server)
