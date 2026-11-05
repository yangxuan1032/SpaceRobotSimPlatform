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
  set(SRSP_RHI_VS "${SRSP_RHI_SHADER_DIR}/triangle.vert.qsb")
  set(SRSP_RHI_FS "${SRSP_RHI_SHADER_DIR}/triangle.frag.qsb")
  add_custom_command(
    OUTPUT "${SRSP_RHI_VS}"
    COMMAND "${QSB_EXECUTABLE}" --qt6 -o "${SRSP_RHI_VS}"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert"
    COMMENT "Compiling triangle.vert.qsb")
  add_custom_command(
    OUTPUT "${SRSP_RHI_FS}"
    COMMAND "${QSB_EXECUTABLE}" --qt6 -o "${SRSP_RHI_FS}"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag"
    COMMENT "Compiling triangle.frag.qsb")
  add_custom_target(srsp_rhi_shaders DEPENDS "${SRSP_RHI_VS}" "${SRSP_RHI_FS}")
  add_dependencies(srsp_render_rhi srsp_rhi_shaders)
  target_compile_definitions(srsp_render_rhi PRIVATE
    SRSP_RHI_VS_PATH="${SRSP_RHI_VS}"
    SRSP_RHI_FS_PATH="${SRSP_RHI_FS}")
else()
  message(STATUS "qsb not found; renderer.rhi draws only the clear pass")
endif()
# Text pipeline assets (B12 step 2b): compiled but not yet bound by the renderer.
set(SRSP_TEXT_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SRSP_TEXT_VS "${CMAKE_CURRENT_BINARY_DIR}/text.vert.qsb")
set(SRSP_TEXT_FS "${CMAKE_CURRENT_BINARY_DIR}/text.frag.qsb")
find_program(QSB_EXECUTABLE NAMES qsb HINTS "${CMAKE_PREFIX_PATH}/bin")
if(QSB_EXECUTABLE)
  add_custom_command(
    OUTPUT "${SRSP_TEXT_VS}"
    COMMAND "${QSB_EXECUTABLE}" --glsl "440" -o "${SRSP_TEXT_VS}" "${SRSP_TEXT_SHADER_DIR}/text.vert"
    DEPENDS "${SRSP_TEXT_SHADER_DIR}/text.vert" VERBATIM)
  add_custom_command(
    OUTPUT "${SRSP_TEXT_FS}"
    COMMAND "${QSB_EXECUTABLE}" --glsl "440" -o "${SRSP_TEXT_FS}" "${SRSP_TEXT_SHADER_DIR}/text.frag"
    DEPENDS "${SRSP_TEXT_SHADER_DIR}/text.frag" VERBATIM)
  add_custom_target(srsp_text_shaders DEPENDS "${SRSP_TEXT_VS}" "${SRSP_TEXT_FS}")
  add_dependencies(srsp_render_rhi srsp_text_shaders)
  target_compile_definitions(srsp_render_rhi PRIVATE
    SRSP_TEXT_VS_PATH="${SRSP_TEXT_VS}"
    SRSP_TEXT_FS_PATH="${SRSP_TEXT_FS}")
  message(STATUS "text shaders: qsb found, compiling text.vert/frag")
else()
  message(STATUS "text shaders: qsb not found, text pipeline assets skipped")
endif()