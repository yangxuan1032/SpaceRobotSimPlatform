add_subdirectory(cli)
add_subdirectory(studio)

if(TARGET srsp_bridge)
  add_subdirectory(sidecar)
else()
  message(STATUS "srsp_bridge is not built; srsp-sidecar is skipped")
endif()
