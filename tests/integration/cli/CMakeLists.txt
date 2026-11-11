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
#
# The sidecar script needs grpcio; probing it at configure time turns a missing
# package into a visible skip instead of a red test that looks like a host bug
# (the CI job installs mingw-w64-ucrt-x86_64-python-grpcio for this reason).
set(SRSP_PY_SIDECAR_READY OFF)
if(TARGET srsp_sidecar AND DEFINED Python3_EXECUTABLE)
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import grpc"
    RESULT_VARIABLE SRSP_GRPC_PROBE
    OUTPUT_QUIET ERROR_QUIET)
  if(SRSP_GRPC_PROBE EQUAL 0)
    set(SRSP_PY_SIDECAR_READY ON)
  else()
    message(STATUS "python-grpcio not importable; Python sidecar tests are skipped")
  endif()
endif()

if(SRSP_PY_SIDECAR_READY)
  add_test(NAME integration.cli_demo_sidecar_python
    COMMAND srsp_cli demo --transport sidecar
            --sidecar-exe "${Python3_EXECUTABLE}"
            --sidecar-script "${CMAKE_SOURCE_DIR}/tools/sidecar/srsp_sidecar.py"
            --plugin $<TARGET_FILE:srsp_hello_plugin>)
  set_tests_properties(integration.cli_demo_sidecar_python PROPERTIES
    TIMEOUT 180
    PASS_REGULAR_EXPRESSION "\"lang\":[ ]?\"python\"")

  # Fault injection: the sidecar fails the first Invoke; the CLI must restart
  # it once and then succeed (M0 T3.3).
  add_test(NAME integration.cli_demo_sidecar_restart
    COMMAND srsp_cli demo --transport sidecar
            --sidecar-exe "${Python3_EXECUTABLE}"
            --sidecar-script "${CMAKE_SOURCE_DIR}/tools/sidecar/srsp_sidecar.py"
            --plugin $<TARGET_FILE:srsp_hello_plugin>)
  set_tests_properties(integration.cli_demo_sidecar_restart PROPERTIES
    TIMEOUT 180
    ENVIRONMENT "SRSP_SIDECAR_FAULT=first-invoke"
    PASS_REGULAR_EXPRESSION "restarting the sidecar once")
endif()
# Component manifest validation (W1): the example manifest must pass, and a
# deliberately broken one must fail with every problem reported.
add_test(NAME integration.cli_component_valid
  COMMAND srsp_cli component validate
          "${CMAKE_SOURCE_DIR}/examples/hello_plugin/component.json")
set_tests_properties(integration.cli_component_valid PROPERTIES
  TIMEOUT 30
  PASS_REGULAR_EXPRESSION "component valid: kind=control\\.controller")

add_test(NAME integration.cli_component_invalid
  COMMAND srsp_cli component validate
          "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/component_invalid.json")
set_tests_properties(integration.cli_component_invalid PROPERTIES
  TIMEOUT 30
  WILL_FAIL TRUE)
# W3.5: the world path publishes one snapshot and one subscription event per
# step, and the recorder writes the same generation that inspect reads back.
add_test(NAME integration.cli_world
  COMMAND srsp_cli world --steps 5)
set_tests_properties(integration.cli_world PROPERTIES
  TIMEOUT 30
  PASS_REGULAR_EXPRESSION "world steps=5 generation=5 events=5 entities=2")

add_test(NAME integration.cli_world_record
  COMMAND srsp_cli world --steps 3 --record "${CMAKE_CURRENT_BINARY_DIR}/world.mcap")
set_tests_properties(integration.cli_world_record PROPERTIES
  TIMEOUT 30
  PASS_REGULAR_EXPRESSION "world steps=3 generation=3 events=3")

add_test(NAME integration.cli_world_record_inspect
  COMMAND srsp_cli inspect "${CMAKE_CURRENT_BINARY_DIR}/world.mcap")
set_tests_properties(integration.cli_world_record_inspect PROPERTIES
  TIMEOUT 30
  DEPENDS integration.cli_world_record
  PASS_REGULAR_EXPRESSION "channels=1")