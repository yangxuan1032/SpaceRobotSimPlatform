/**
 * @file test_sidecar_roundtrip.cpp
 * @brief Conformance test: the same plugin call over inproc and sidecar must
 *        produce byte-identical payloads.
 *
 * Usage: test_sidecar_roundtrip <plugin-path>
 * Exit code 0 on success; non-zero with a message on the first failure.
 */
#include <srsp/bridge/sidecar_host.hpp>
#include <srsp/bridge/sidecar_server.hpp>
#include <srsp/core/plugin_host.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
#  include <malloc.h>
#endif

namespace {

int fail(const char* what) {
  std::fprintf(stderr, "FAIL: %s\n", what);
  return 1;
}

void SRSP_CALL t_log(void*, int32_t, srsp_str) {}

double SRSP_CALL t_time(void*) { return 0.0; }

int64_t SRSP_CALL t_time_ns(void*) { return 0; }

srsp_status SRSP_CALL t_alloc(void*, uint64_t size, uint64_t alignment,
                              void** out) {
  if (out == nullptr || size == 0) {
    return SRSP_E_INVALID_ARGUMENT;
  }
  if (alignment < alignof(void*)) {
    alignment = alignof(void*);
  }
#if defined(_WIN32)
  *out = _aligned_malloc(static_cast<size_t>(size), static_cast<size_t>(alignment));
#else
  if (posix_memalign(out, static_cast<size_t>(alignment),
                     static_cast<size_t>(size)) != 0) {
    *out = nullptr;
  }
#endif
  return *out != nullptr ? SRSP_OK : SRSP_E_OUT_OF_MEMORY;
}

void SRSP_CALL t_free(void*, void* ptr) {
#if defined(_WIN32)
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

srsp_status SRSP_CALL t_publish(void*, srsp_str, srsp_bytes, uint64_t) {
  return SRSP_E_UNSUPPORTED;
}

srsp_status SRSP_CALL t_shm_map(void*, srsp_str, uint64_t, int32_t, void**) {
  return SRSP_E_UNSUPPORTED;
}

void SRSP_CALL t_shm_unmap(void*, void*, uint64_t) {}

srsp_host_v1 services() {
  srsp_host_v1 table{};
  table.struct_size = sizeof(srsp_host_v1);
  table.log = t_log;
  table.time_tai = t_time;
  table.time_tai_ns = t_time_ns;
  table.alloc = t_alloc;
  table.free = t_free;
  table.publish = t_publish;
  table.shm_map = t_shm_map;
  table.shm_unmap = t_shm_unmap;
  return table;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fputs("usage: test_sidecar_roundtrip <plugin-path>\n", stderr);
    return 2;
  }
  const std::string plugin_path = argv[1];

  // Reference: call the plugin directly in this process.
  srsp::core::PluginHost local(services());
  if (local.load(plugin_path, nullptr) != SRSP_OK) {
    return fail("inproc load failed");
  }
  if (local.init(srsp_str{}, nullptr) != SRSP_OK) {
    return fail("inproc init failed");
  }
  srsp_buffer local_response{};
  if (local.call(srsp_str{"core.ping", 9}, srsp_bytes{}, &local_response,
                 nullptr) != SRSP_OK) {
    return fail("inproc call failed");
  }
  const std::string expected(
      reinterpret_cast<const char*>(local_response.data), local_response.size);
  local.unload();

  // Sidecar: serve the same plugin over gRPC and call it through the bridge.
  const std::string token = "m0-token";
  srsp::bridge::SidecarServer server;
  if (!server.start("127.0.0.1:0", token, plugin_path)) {
    return fail("sidecar server did not start");
  }

  srsp::bridge::SidecarHost client(server.address(), token, 3000);
  srsp_error err{};
  if (client.connect(&err) != SRSP_OK) {
    server.stop();
    return fail("handshake failed");
  }
  if (!client.ping(0)) {
    server.stop();
    return fail("ping failed");
  }

  std::string actual;
  if (client.invoke("core.ping", std::string{}, &actual, &err) != SRSP_OK) {
    server.stop();
    return fail("invoke failed");
  }
  if (actual != expected) {
    std::fprintf(stderr, "FAIL: payload mismatch\n  inproc : %s\n  sidecar: %s\n",
                 expected.c_str(), actual.c_str());
    server.stop();
    return 1;
  }

  client.shutdown();
  server.stop();
  std::printf("PASS: inproc and sidecar agree on %zu bytes\n", actual.size());
  return 0;
}
