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
# Shaders are compiled to .qsb at build time (qsb ships with qt6-shadertools).
find_program(QSB_EXECUTABLE NAMES qsb HINTS "${QT6_INSTALL_PREFIX}/bin" "C:/msys64/ucrt64/bin")
if(QSB_EXECUTABLE)
  set(SRSP_RHI_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  set(SRSP_RHI_SHADER "${SRSP_RHI_SHADER_DIR}/triangle.qsb")
  add_custom_command(
    OUTPUT "${SRSP_RHI_SHADER}"
    COMMAND "${QSB_EXECUTABLE}" --qt6 -o "${SRSP_RHI_SHADER}"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag"
    COMMENT "Compiling triangle.qsb")
  add_custom_target(srsp_rhi_shaders DEPENDS "${SRSP_RHI_SHADER}")
  add_dependencies(srsp_render_rhi srsp_rhi_shaders)
  target_compile_definitions(srsp_render_rhi PRIVATE
    SRSP_RHI_SHADER_PATH="${SRSP_RHI_SHADER}")
else()
  message(STATUS "qsb not found; renderer.rhi draws only the clear pass")
endif()