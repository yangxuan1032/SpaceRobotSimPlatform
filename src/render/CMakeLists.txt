#
# srsp_render: renderer contract plus the headless null renderer.
# The bgfx backend will be added as a separate target once it is built.
#
add_library(srsp_render STATIC
  src/null_renderer.cpp
  src/projection.cpp)
target_include_directories(srsp_render PUBLIC include)
set_target_properties(srsp_render PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
  target_compile_options(srsp_render PRIVATE /W4)
else()
  target_compile_options(srsp_render PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(backends/qt)

add_subdirectory(backends/rhi)
