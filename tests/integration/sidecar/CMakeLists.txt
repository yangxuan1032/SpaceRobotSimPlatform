add_executable(test_sidecar_roundtrip test_sidecar_roundtrip.cpp)
target_link_libraries(test_sidecar_roundtrip PRIVATE srsp_bridge)

if(TARGET srsp_hello_plugin)
  add_dependencies(test_sidecar_roundtrip srsp_hello_plugin)
  add_test(NAME integration.sidecar_roundtrip
    COMMAND test_sidecar_roundtrip $<TARGET_FILE:srsp_hello_plugin>)
  set_tests_properties(integration.sidecar_roundtrip PROPERTIES
    TIMEOUT 60
    PASS_REGULAR_EXPRESSION "PASS: inproc and sidecar agree")
else()
  message(STATUS "srsp_hello_plugin not built; sidecar roundtrip is not registered")
endif()
