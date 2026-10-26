# Headless renderer self-test; Qt DLLs come from the UCRT64 prefix on Windows.
add_test(NAME integration.studio_selftest
  COMMAND srsp_studio --selftest)
set_tests_properties(integration.studio_selftest PROPERTIES
  TIMEOUT 60
  PASS_REGULAR_EXPRESSION "PASS: renderer contract ok")

if(WIN32)
  set_tests_properties(integration.studio_selftest PROPERTIES
    ENVIRONMENT "PATH=C:/msys64/ucrt64/bin;$ENV{PATH}")
endif()
# Four-panel UI wiring, offscreen Qt platform.
add_test(NAME integration.studio_ui
  COMMAND srsp_studio --selftest-ui "${CMAKE_SOURCE_DIR}/examples/models/arm6.srm.json")
set_tests_properties(integration.studio_ui PROPERTIES
  TIMEOUT 60
  PASS_REGULAR_EXPRESSION "PASS: studio panels wired")

if(WIN32)
  set_tests_properties(integration.studio_ui PROPERTIES
    ENVIRONMENT "PATH=C:/msys64/ucrt64/bin;$ENV{PATH}")
endif()
