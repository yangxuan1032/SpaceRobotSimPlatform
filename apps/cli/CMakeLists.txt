add_executable(srsp_cli main.cpp)
target_link_libraries(srsp_cli PRIVATE srsp_core)
set_target_properties(srsp_cli PROPERTIES OUTPUT_NAME srsp-cli)

if(MSVC)
  target_compile_options(srsp_cli PRIVATE /W4)
else()
  target_compile_options(srsp_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()
