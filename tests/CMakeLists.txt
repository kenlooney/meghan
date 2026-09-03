add_executable(smoke smoke.c)
target_link_libraries(smoke PRIVATE meglang)
add_test(NAME smoke COMMAND smoke)
add_test(NAME cli_help COMMAND meg --help)
