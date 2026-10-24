add_executable(test_host_api test_host_api.c)
target_link_libraries(test_host_api PRIVATE srsp_core)

if(TARGET srsp_hello_plugin)
  add_dependencies(test_host_api srsp_hello_plugin)
  add_test(NAME integration.host_api_c
    COMMAND test_host_api $<TARGET_FILE:srsp_hello_plugin>)
  set_tests_properties(integration.host_api_c PROPERTIES
    TIMEOUT 30
    PASS_REGULAR_EXPRESSION "PASS: core_api works from C")
else()
  message(STATUS "srsp_hello_plugin not built; integration.host_api_c is not registered")
endif()
