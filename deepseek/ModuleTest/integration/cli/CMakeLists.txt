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

add_test(NAME integration.cli_sidecar_reserved
  COMMAND srsp_cli demo --transport sidecar)
set_tests_properties(integration.cli_sidecar_reserved PROPERTIES
  TIMEOUT 30
  PASS_REGULAR_EXPRESSION "reserved for M0 stage 3")
