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

# Host-side C API: proves the kernel is drivable from plain C. MSVC's C mode
# currently rejects vcruntime.h when compiling with the same include layout,
# so the test runs on GCC/Clang (UCRT64) and is tracked as a CI item.
if(TARGET srsp_core AND NOT MSVC)
  add_subdirectory(host_api)
elseif(TARGET srsp_core)
  message(STATUS "integration.host_api_c is skipped on MSVC (C-mode header conflict)")
endif()
