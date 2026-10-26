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
