add_executable(srsp_cli main.cpp)
target_link_libraries(srsp_cli PRIVATE srsp_core)
set_target_properties(srsp_cli PROPERTIES OUTPUT_NAME srsp-cli)

# The sidecar transport is compiled in only when the gRPC bridge exists.
if(TARGET srsp_bridge)
  target_link_libraries(srsp_cli PRIVATE srsp_bridge)
  target_compile_definitions(srsp_cli PRIVATE SRSP_HAVE_SIDECAR=1)
endif()

if(MSVC)
  target_compile_options(srsp_cli PRIVATE /W4)
else()
  target_compile_options(srsp_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()
