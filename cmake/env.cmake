#
# Single source of truth for "which environment are we building in".
#
# SRSP_ENV is one of: ucrt64 | clang64 | msvc | linux | windows-unknown.
# Dependency gates must use SRSP_ENV_DEPS_OK instead of guessing from whether a
# package happens to be found: that guess leaked MSYS2 (GCC) headers into MSVC
# builds (issue P27) and made CLANG64 look supported while it was not (P28).
#
set(SRSP_ENV "linux")
if(WIN32)
  if(MSVC)
    set(SRSP_ENV "msvc")
  elseif(CMAKE_CXX_COMPILER MATCHES "clang64")
    set(SRSP_ENV "clang64")
  elseif(CMAKE_CXX_COMPILER MATCHES "ucrt64")
    set(SRSP_ENV "ucrt64")
  else()
    set(SRSP_ENV "windows-unknown")
  endif()
endif()

# MSYS2/pkg-config style dependencies are only reliable in UCRT64 and on Linux.
# CLANG64 uses libc++ and mixes prefixes; MSVC must never see the MSYS2 prefix.
set(SRSP_ENV_DEPS_OK OFF)
if(SRSP_ENV STREQUAL "ucrt64" OR SRSP_ENV STREQUAL "linux")
  set(SRSP_ENV_DEPS_OK ON)
endif()

set(SRSP_ENV "${SRSP_ENV}" CACHE INTERNAL "Identified build environment")
set(SRSP_ENV_DEPS_OK "${SRSP_ENV_DEPS_OK}" CACHE INTERNAL
    "MSYS2/apt dependencies are usable in this environment")
message(STATUS "SRSP environment: ${SRSP_ENV} (dependencies available: ${SRSP_ENV_DEPS_OK})")

# Optional build cache: enables ccache for C and C++ when it is installed.
# Keep it switchable so CI or a bisect can disable it without editing files.
option(SRSP_USE_CCACHE "Use ccache for C/C++ compilation when available" ON)
if(SRSP_USE_CCACHE)
  find_program(SRSP_CCACHE_PROGRAM ccache)
  if(SRSP_CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${SRSP_CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${SRSP_CCACHE_PROGRAM}")
    message(STATUS "ccache enabled: ${SRSP_CCACHE_PROGRAM}")
  else()
    message(STATUS "ccache not found; building without a compiler cache")
  endif()
endif()