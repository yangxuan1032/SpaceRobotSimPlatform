#
# srsp_core: kernel library (plugin host, registry, diagnostics).
# Static for now; the shared-library export set is added when the public C
# kernel API (core_api.h) is implemented.
#
add_library(srsp_core STATIC
  src/diagnostics.cpp
  src/plugin_host.cpp
  src/registry.cpp
  src/scheduler.cpp
  src/time_service.cpp)

target_include_directories(srsp_core PUBLIC include)
target_link_libraries(srsp_core PUBLIC SRSP::api ${CMAKE_DL_LIBS})

if(MSVC)
  target_compile_options(srsp_core PRIVATE /W4)
else()
  target_compile_options(srsp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
