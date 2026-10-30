add_executable(srsp_cli main.cpp)
target_link_libraries(srsp_cli PRIVATE srsp_core)

# The soak command can drive the real solver; keep it optional so toolchains
# without the model/dynamics dependencies still build the CLI.
if(TARGET srsp_dynamics AND TARGET srsp_model)
  target_link_libraries(srsp_cli PRIVATE srsp_dynamics srsp_model)
  target_compile_definitions(srsp_cli PRIVATE SRSP_HAVE_SOLVER=1)
endif()
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
