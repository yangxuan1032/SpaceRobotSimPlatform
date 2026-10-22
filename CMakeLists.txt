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

option(SRSP_BUILD_EXAMPLES "Build the example plugins and host" ON)
option(SRSP_BUILD_TESTS "Build the test suite" ON)
option(SRSP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

enable_testing()

if(SRSP_BUILD_EXAMPLES)
  add_subdirectory(examples/hello_plugin)
endif()

if(SRSP_BUILD_TESTS)
  add_subdirectory(tests)
endif()

# Placeholder: core, plugins and apps are added milestone by milestone.
# add_subdirectory(core)
# add_subdirectory(plugins)
# add_subdirectory(apps)
