# Module tests need Catch2 built with the same C++ runtime as the active
# toolchain. It is currently provided by MSYS2 UCRT64 only, so the tests are
# enabled for the UCRT64 toolchain (GCC or Clang) and skipped elsewhere.
# See tools/ai/skills/module-test/SKILL.md.
if(MSVC OR NOT CMAKE_CXX_COMPILER MATCHES "ucrt64")
  message(STATUS "Module tests are enabled only for the UCRT64 toolchain; skipped for the current compiler")
  return()
endif()

find_package(Catch2 3 CONFIG QUIET)
if(NOT Catch2_FOUND)
  message(STATUS "Catch2 not found; module tests are disabled (install mingw-w64-ucrt-x86_64-catch)")
  return()
endif()

add_subdirectory(unit)
