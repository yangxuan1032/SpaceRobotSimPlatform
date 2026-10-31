# The shell embeds SRM loading and rigid-body playback, so it only builds on
# toolchains that provide those libraries (Qt alone is not enough).
if(NOT TARGET srsp_model OR NOT TARGET srsp_dynamics)
  message(STATUS "srsp_model or srsp_dynamics is unavailable; srsp-studio is skipped")
  return()
endif()

find_package(Qt6 COMPONENTS Widgets Gui QUIET)
if(NOT Qt6_FOUND)
  message(STATUS "Qt6 not found; srsp-studio is skipped")
  return()
endif()

# Renderer library shared by the shell and the offscreen renderer test.
add_library(srsp_studio_render STATIC
  studio_window.cpp)
target_include_directories(srsp_studio_render PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(srsp_studio_render PUBLIC srsp_render srsp_render_qt srsp_dynamics srsp_model Qt6::Widgets)

qt_add_executable(srsp_studio main.cpp)
set_target_properties(srsp_studio PROPERTIES OUTPUT_NAME srsp-studio)
target_link_libraries(srsp_studio PRIVATE srsp_studio_render Qt6::Widgets)

if(MSVC)
  target_compile_options(srsp_studio_render PRIVATE /W4)
  target_compile_options(srsp_studio PRIVATE /W4)
else()
  target_compile_options(srsp_studio_render PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(srsp_studio PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The RHI backend is optional; studio_window.cpp lives in srsp_studio_render,
# so the dependency belongs to that library.
if(TARGET srsp_render_rhi)
  target_link_libraries(srsp_studio_render PUBLIC srsp_render_rhi)
  target_include_directories(srsp_studio_render PUBLIC
    "${PROJECT_SOURCE_DIR}/src/render/backends/rhi")
endif()
