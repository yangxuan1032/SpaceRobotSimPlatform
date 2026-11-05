#
# srsp_render: renderer contract plus the headless null renderer.
# The bgfx backend will be added as a separate target once it is built.
#
add_library(srsp_render STATIC
  src/null_renderer.cpp
  src/projection.cpp)

# GLB mesh reading needs nlohmann (the same JSON library the model layer uses).
if(TARGET nlohmann_json::nlohmann_json)
  target_sources(srsp_render PRIVATE src/mesh_loader.cpp)
  target_link_libraries(srsp_render PUBLIC nlohmann_json::nlohmann_json)
  target_compile_definitions(srsp_render PUBLIC SRSP_HAVE_MESH_LOADER=1)
endif()
target_include_directories(srsp_render PUBLIC include)
set_target_properties(srsp_render PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
  target_compile_options(srsp_render PRIVATE /W4)
else()
  target_compile_options(srsp_render PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(backends/qt)

add_subdirectory(backends/rhi)

add_subdirectory(backends/factory)
