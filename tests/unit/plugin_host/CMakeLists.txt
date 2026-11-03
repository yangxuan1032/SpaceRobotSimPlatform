add_executable(test_plugin_host test_plugin_host.cpp)
target_link_libraries(test_plugin_host PRIVATE srsp_kernel Catch2::Catch2WithMain)

if(TARGET srsp_hello_plugin)
  add_dependencies(test_plugin_host srsp_hello_plugin)
  add_test(NAME unit.plugin_host COMMAND test_plugin_host)
  set_tests_properties(unit.plugin_host PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "SRSP_TEST_PLUGIN=$<TARGET_FILE:srsp_hello_plugin>")
else()
  message(STATUS "srsp_hello_plugin not built; unit.plugin_host is not registered")
endif()

add_executable(test_manifest_load test_manifest_load.cpp)
target_link_libraries(test_manifest_load PRIVATE srsp_kernel Catch2::Catch2WithMain)

if(TARGET srsp_hello_plugin AND SRSP_MANIFEST_ENABLED)
  add_dependencies(test_manifest_load srsp_hello_plugin)
  # The manifest uses install-layout relative paths (bin/<lib>), so the copy
  # must sit in the plugin's install directory in the build tree.
  set(SRSP_MANIFEST_COPY_DIR
    "${CMAKE_BINARY_DIR}/plugins/org.srsp.examples.hello")
  configure_file(
    "${CMAKE_SOURCE_DIR}/examples/hello_plugin/plugin.json"
    "${SRSP_MANIFEST_COPY_DIR}/plugin.json"
    COPYONLY)
  add_test(NAME unit.manifest_load COMMAND test_manifest_load)
  set_tests_properties(unit.manifest_load PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "SRSP_TEST_MANIFEST=${SRSP_MANIFEST_COPY_DIR}/plugin.json")
else()
  message(STATUS "manifest parsing disabled for this toolchain; unit.manifest_load is not registered")
endif()

if(MSVC)
  target_compile_options(test_plugin_host PRIVATE /W4)
  target_compile_options(test_manifest_load PRIVATE /W4)
else()
  target_compile_options(test_plugin_host PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(test_manifest_load PRIVATE -Wall -Wextra -Wpedantic)
endif()
