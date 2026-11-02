# renderer factory: one place that maps a runtime choice to a backend.
add_library(srsp_render_factory STATIC renderer_factory.cpp)
target_include_directories(srsp_render_factory PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(srsp_render_factory PUBLIC srsp_render)

if(TARGET srsp_render_qt)
  target_link_libraries(srsp_render_factory PUBLIC srsp_render_qt)
  target_compile_definitions(srsp_render_factory PRIVATE SRSP_RENDER_FACTORY_HAVE_QT=1)
endif()
if(TARGET srsp_render_rhi)
  target_link_libraries(srsp_render_factory PUBLIC srsp_render_rhi)
  target_compile_definitions(srsp_render_factory PRIVATE SRSP_RENDER_FACTORY_HAVE_RHI=1)
endif()

if(MSVC)
  target_compile_options(srsp_render_factory PRIVATE /W4)
else()
  target_compile_options(srsp_render_factory PRIVATE -Wall -Wextra -Wpedantic)
endif()