#
# srsp_proto: generated protobuf and gRPC stubs for the plugin runtime.
# srsp_bridge: sidecar host (client) and sidecar server.
#
# Both packages must provide CMake config files: `protobuf_generate` comes from
# Protobuf's own config, and gRPC::grpc++ from gRPC's. Distributions that only
# ship pkg-config files (for example Ubuntu's libprotobuf-dev) cannot be used,
# so the bridge degrades to "disabled" instead of failing configuration.
#
find_package(Protobuf CONFIG QUIET)
find_package(gRPC CONFIG QUIET)
if(NOT Protobuf_FOUND OR NOT gRPC_FOUND)
  message(STATUS "protobuf/gRPC CMake config not found; the sidecar bridge is disabled")
  return()
endif()

set(SRSP_PROTO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../api/proto")
set(SRSP_PROTO_FILE "${SRSP_PROTO_ROOT}/srsp/v1/plugin.proto")

add_library(srsp_proto STATIC "${SRSP_PROTO_FILE}")
target_link_libraries(srsp_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)
target_include_directories(srsp_proto PUBLIC
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${SRSP_PROTO_ROOT}")

protobuf_generate(TARGET srsp_proto
  LANGUAGE cpp
  IMPORT_DIRS "${SRSP_PROTO_ROOT}")
protobuf_generate(TARGET srsp_proto
  LANGUAGE grpc
  GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
  PLUGIN "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
  IMPORT_DIRS "${SRSP_PROTO_ROOT}")

add_library(srsp_bridge STATIC
  src/sidecar_host.cpp
  src/sidecar_process.cpp
  src/sidecar_server.cpp)
target_include_directories(srsp_bridge PUBLIC include)
target_link_libraries(srsp_bridge PUBLIC srsp_kernel srsp_proto)

if(MSVC)
  target_compile_options(srsp_bridge PRIVATE /W4)
else()
  target_compile_options(srsp_bridge PRIVATE -Wall -Wextra -Wpedantic)
endif()
