add_executable(test_plugin_host test_plugin_host.cpp)
target_link_libraries(test_plugin_host PRIVATE srsp_core Catch2::Catch2WithMain)

if(TARGET srsp_hello_plugin)
  add_dependencies(test_plugin_host srsp_hello_plugin)
  add_test(NAME unit.plugin_host COMMAND test_plugin_host)
  set_tests_properties(unit.plugin_host PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "SRSP_TEST_PLUGIN=$<TARGET_FILE:srsp_hello_plugin>")
else()
  message(STATUS "srsp_hello_plugin not built; unit.plugin_host is not registered")
endif()

if(MSVC)
  target_compile_options(test_plugin_host PRIVATE /W4)
else()
  target_compile_options(test_plugin_host PRIVATE -Wall -Wextra -Wpedantic)
endif()
