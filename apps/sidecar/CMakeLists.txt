add_executable(srsp_sidecar main.cpp)
target_link_libraries(srsp_sidecar PRIVATE srsp_bridge)
set_target_properties(srsp_sidecar PROPERTIES OUTPUT_NAME srsp-sidecar)

if(MSVC)
  target_compile_options(srsp_sidecar PRIVATE /W4)
else()
  target_compile_options(srsp_sidecar PRIVATE -Wall -Wextra -Wpedantic)
endif()
