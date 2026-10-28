# renderer.rhi: GPU backend on top of Qt RHI. Offscreen only for now, so it
# needs no window; D3D11 is preferred and the Null backend keeps CI headless.
find_package(Qt6 COMPONENTS Gui GuiPrivate QUIET)
if(NOT Qt6GuiPrivate_FOUND)
  message(STATUS "Qt6 GuiPrivate not found; renderer.rhi is skipped")
  return()
endif()

add_library(srsp_render_rhi STATIC rhi_renderer.cpp)
target_include_directories(srsp_render_rhi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(srsp_render_rhi PUBLIC srsp_render Qt6::Gui Qt6::GuiPrivate)
set_target_properties(srsp_render_rhi PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
  target_compile_options(srsp_render_rhi PRIVATE /W4)
else()
  target_compile_options(srsp_render_rhi PRIVATE -Wall -Wextra -Wpedantic)
endif()