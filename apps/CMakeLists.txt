add_subdirectory(cli)

if(TARGET srsp_bridge)
  add_subdirectory(sidecar)
else()
  message(STATUS "srsp_bridge is not built; srsp-sidecar is skipped")
endif()
