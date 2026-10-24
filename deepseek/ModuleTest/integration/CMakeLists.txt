# Integration tests: cross-module flows. They only need the CLI and the
# example plugin, so they run on every toolchain (no Catch2 required).
if(TARGET srsp_cli AND TARGET srsp_hello_plugin)
  add_subdirectory(cli)
else()
  message(STATUS "srsp_cli or srsp_hello_plugin missing; integration tests are skipped")
endif()

# The sidecar roundtrip needs gRPC, so it only appears when the bridge is built.
if(TARGET srsp_bridge)
  add_subdirectory(sidecar)
endif()
