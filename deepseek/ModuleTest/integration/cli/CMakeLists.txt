add_test(NAME integration.cli_version
  COMMAND srsp_cli version)
set_tests_properties(integration.cli_version PROPERTIES
  TIMEOUT 30
  PASS_REGULAR_EXPRESSION "srsp-cli 0\\.1\\.0")

add_test(NAME integration.cli_demo_inproc
  COMMAND srsp_cli demo --transport inproc
          --plugin $<TARGET_FILE:srsp_hello_plugin>)
set_tests_properties(integration.cli_demo_inproc PROPERTIES
  TIMEOUT 30
  PASS_REGULAR_EXPRESSION "\"ok\":true")

add_test(NAME integration.cli_demo_unknown_method
  COMMAND srsp_cli demo --transport inproc
          --plugin $<TARGET_FILE:srsp_hello_plugin> --method nope.missing)
set_tests_properties(integration.cli_demo_unknown_method PROPERTIES
  TIMEOUT 30
  WILL_FAIL TRUE)

# The same call over the sidecar transport must produce the same payload.
if(TARGET srsp_sidecar)
  add_test(NAME integration.cli_demo_sidecar
    COMMAND srsp_cli demo --transport sidecar
            --sidecar-exe $<TARGET_FILE:srsp_sidecar>
            --plugin $<TARGET_FILE:srsp_hello_plugin>)
  set_tests_properties(integration.cli_demo_sidecar PROPERTIES
    TIMEOUT 120
    PASS_REGULAR_EXPRESSION "\"ok\":true")
else()
  message(STATUS "srsp_sidecar not built; integration.cli_demo_sidecar is not registered")
endif()

# Cross-language path: a Python sidecar driven by the same C++ host.
if(TARGET srsp_sidecar AND DEFINED Python3_EXECUTABLE)
  add_test(NAME integration.cli_demo_sidecar_python
    COMMAND srsp_cli demo --transport sidecar
            --sidecar-exe "${Python3_EXECUTABLE}"
            --sidecar-script "${CMAKE_SOURCE_DIR}/tools/sidecar/srsp_sidecar.py"
            --plugin $<TARGET_FILE:srsp_hello_plugin>)
  set_tests_properties(integration.cli_demo_sidecar_python PROPERTIES
    TIMEOUT 180
    PASS_REGULAR_EXPRESSION "\"lang\":[ ]?\"python\"")
endif()
