# Integration tests need only the CLI and the example plugin, so they run on
# every toolchain. Unit tests need Catch2 built with the same C++ runtime as
# the active toolchain, which is currently provided by MSYS2 UCRT64 only.
# See tools/ai/skills/module-test/SKILL.md.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/CMakeLists.txt")
  add_subdirectory(integration)
endif()

if(MSVC OR NOT CMAKE_CXX_COMPILER MATCHES "ucrt64")
  message(STATUS "Unit tests are enabled only for the UCRT64 toolchain; skipped for the current compiler")
  return()
endif()

find_package(Catch2 3 CONFIG QUIET)
if(NOT Catch2_FOUND)
  message(STATUS "Catch2 not found; unit tests are disabled (install mingw-w64-ucrt-x86_64-catch)")
  return()
endif()

add_subdirectory(unit)
