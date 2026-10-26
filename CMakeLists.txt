cmake_minimum_required(VERSION 3.27)

project(SRSP VERSION 0.1.0 LANGUAGES C CXX)

# Every build must be owned by a model namespace so caches stay under
# <model>/Temp (see AGENTS.md and ai-models.json).
if(NOT DEFINED ENV{SRSP_AI_MODEL} OR "$ENV{SRSP_AI_MODEL}" STREQUAL "")
  message(FATAL_ERROR
    "SRSP_AI_MODEL is not set. Run '. tools/ai/Enter-AgentEnv.ps1 -Model <slug>' "
    "in PowerShell, or 'export SRSP_AI_MODEL=<slug>' in bash. "
    "Known namespaces: deepseek, claude, gpt, gemini, qwen, glm, agent, ci, local.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Public contract surface: headers only, no third-party dependencies.
add_library(srsp_api INTERFACE)
add_library(SRSP::api ALIAS srsp_api)
target_include_directories(srsp_api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/api/include")
target_compile_features(srsp_api INTERFACE cxx_std_20)

option(SRSP_BUILD_CORE "Build the kernel library" ON)
option(SRSP_BUILD_MODEL "Build the SRM model layer" ON)
option(SRSP_BUILD_DYNAMICS "Build the rigid-body dynamics library" ON)
option(SRSP_BUILD_RENDER "Build the renderer contract and backends" ON)
option(SRSP_BUILD_RECORD "Build MCAP recording and state hashing" ON)
option(SRSP_BUILD_APPS "Build the command line tools" ON)
option(SRSP_BUILD_BRIDGE "Build the gRPC sidecar bridge when gRPC is available" ON)
option(SRSP_BUILD_EXAMPLES "Build the example plugins and host" ON)
option(SRSP_BUILD_TESTS "Build the test suite" ON)
option(SRSP_BUILD_BENCH "Build the benchmarks" ON)
option(SRSP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

enable_testing()

if(SRSP_BUILD_CORE)
  add_subdirectory(src/core)
endif()

# SRM needs nlohmann-json, the same dependency the manifest parser uses; the
# cache variable set by src/core tells whether it is available for this
# toolchain (UCRT64 or MSVC/Linux, not CLANG64).
if(SRSP_BUILD_MODEL AND SRSP_MANIFEST_ENABLED)
  add_subdirectory(src/model)
elseif(SRSP_BUILD_MODEL)
  message(STATUS "nlohmann-json is unavailable for this toolchain; the SRM model layer is disabled")
endif()

if(SRSP_BUILD_RENDER)
  add_subdirectory(src/render)
endif()

if(SRSP_BUILD_RECORD AND NOT MSVC)
  add_subdirectory(src/record)
endif()

# The gRPC bridge is currently supported on the UCRT64 toolchain only: the
# gRPC package lives in the UCRT64 prefix and its headers conflict with the
# CLANG64 libc++ toolchain; MSVC has no gRPC package in this workspace.
# It is added before apps so that the CLI can link it when available.
if(SRSP_BUILD_BRIDGE AND NOT MSVC AND (NOT WIN32 OR CMAKE_CXX_COMPILER MATCHES "ucrt64"))
  find_package(gRPC CONFIG QUIET)
  if(gRPC_FOUND)
    add_subdirectory(src/bridge)
  else()
    message(STATUS "gRPC not found; the sidecar bridge is disabled")
  endif()
elseif(SRSP_BUILD_BRIDGE)
  message(STATUS "The sidecar bridge is enabled only for the UCRT64 toolchain; skipped for this compiler")
endif()

if(SRSP_BUILD_DYNAMICS AND TARGET srsp_model AND NOT MSVC)
  add_subdirectory(src/dynamics)
elseif(SRSP_BUILD_DYNAMICS)
  message(STATUS "SRM is unavailable; the dynamics library is disabled")
endif()

if(SRSP_BUILD_APPS)
  add_subdirectory(apps)
endif()

if(SRSP_BUILD_EXAMPLES)
  add_subdirectory(examples/hello_plugin)
endif()

if(SRSP_BUILD_BENCH AND TARGET srsp_dynamics AND TARGET srsp_record)
  add_subdirectory(bench)
elseif(SRSP_BUILD_BENCH)
  message(STATUS "dynamics or record is unavailable; benchmarks are disabled")
endif()

if(SRSP_BUILD_TESTS)
  add_subdirectory(tests)
endif()

# Model-scoped development tests: <model>/ModuleTest (see tools/ai/skills/module-test).
if(SRSP_BUILD_TESTS AND DEFINED ENV{SRSP_AI_MODEL} AND NOT "$ENV{SRSP_AI_MODEL}" STREQUAL "")
  set(SRSP_MODULE_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/$ENV{SRSP_AI_MODEL}/ModuleTest")
  if(EXISTS "${SRSP_MODULE_TEST_DIR}/CMakeLists.txt")
    add_subdirectory("${SRSP_MODULE_TEST_DIR}" "${CMAKE_BINARY_DIR}/module_test")
  endif()
endif()

# Placeholder: plugins are added milestone by milestone.
# add_subdirectory(plugins)
