#
# Kernel libraries:
#   srsp_kernel - internal C++ implementation (plugin host, registry, clock,
#                 scheduler, diagnostics). Not an ABI surface.
#   srsp_core   - shared library exposing only the host-side C API
#                 (core_api.h), so Rust/Python/Web hosts can bind to it.
#
# Manifest parsing needs nlohmann-json. The package is only available in the
# UCRT64 prefix in this workspace, and pulling its headers into CLANG64 would
# mix UCRT64 headers with libc++; MSVC has no package either. The feature is
# therefore enabled per toolchain and reported as SRSP_MANIFEST_ENABLED.
#
set(SRSP_MANIFEST_ENABLED OFF)
if(MSVC OR NOT WIN32 OR CMAKE_CXX_COMPILER MATCHES "ucrt64")
  find_package(nlohmann_json CONFIG QUIET)
  if(nlohmann_json_FOUND)
    set(SRSP_MANIFEST_ENABLED ON)
  endif()
endif()
set(SRSP_MANIFEST_ENABLED ${SRSP_MANIFEST_ENABLED} CACHE INTERNAL
    "Plugin manifest parsing is enabled")

add_library(srsp_kernel STATIC
  src/diagnostics.cpp
  src/plugin_host.cpp
  src/registry.cpp
  src/scheduler.cpp
  src/time_service.cpp)

target_include_directories(srsp_kernel PUBLIC include)
target_link_libraries(srsp_kernel PUBLIC SRSP::api ${CMAKE_DL_LIBS})
# srsp_core links this archive into a shared library, so the objects must be
# position independent on ELF targets (no-op on Windows).
set_target_properties(srsp_kernel PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(SRSP_MANIFEST_ENABLED)
  target_link_libraries(srsp_kernel PUBLIC nlohmann_json::nlohmann_json)
  target_compile_definitions(srsp_kernel PUBLIC SRSP_HAVE_MANIFEST=1)
endif()

add_library(srsp_core SHARED src/core_api.cpp)
target_include_directories(srsp_core PUBLIC include)
target_link_libraries(srsp_core PUBLIC SRSP::api PRIVATE srsp_kernel)
target_compile_definitions(srsp_core PRIVATE SRSP_CORE_BUILD=1)

if(MSVC)
  target_compile_options(srsp_kernel PRIVATE /W4)
  target_compile_options(srsp_core PRIVATE /W4)
else()
  target_compile_options(srsp_kernel PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(srsp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
