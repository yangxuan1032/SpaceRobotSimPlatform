/**
 * @file test_plugin_host.cpp
 * @brief Unit tests for srsp::core::PluginHost using the hello plugin fixture.
 *
 * The plugin path is provided by ctest through the SRSP_TEST_PLUGIN
 * environment variable.
 */
#include <srsp/core/plugin_host.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#  include <malloc.h>
#endif

namespace {

int g_log_count = 0;

void SRSP_CALL test_log(void*, int32_t level, srsp_str message) {
  (void)level;
  (void)message;
  ++g_log_count;
}

double SRSP_CALL test_time(void*) { return 0.0; }

int64_t SRSP_CALL test_time_ns(void*) { return 0; }

srsp_status SRSP_CALL test_alloc(void*, uint64_t size, uint64_t alignment,
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

void SRSP_CALL test_free(void*, void* ptr) {
#if defined(_WIN32)
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

srsp_status SRSP_CALL test_publish(void*, srsp_str, srsp_bytes, uint64_t) {
  return SRSP_E_UNSUPPORTED;
}

srsp_status SRSP_CALL test_shm_map(void*, srsp_str, uint64_t, int32_t, void**) {
  return SRSP_E_UNSUPPORTED;
}

void SRSP_CALL test_shm_unmap(void*, void*, uint64_t) {}

srsp_host_v1 make_services() {
  srsp_host_v1 services{};
  services.struct_size = sizeof(srsp_host_v1);
  services.user = nullptr;
  services.log = test_log;
  services.time_tai = test_time;
  services.time_tai_ns = test_time_ns;
  services.alloc = test_alloc;
  services.free = test_free;
  services.publish = test_publish;
  services.shm_map = test_shm_map;
  services.shm_unmap = test_shm_unmap;
  return services;
}

std::string plugin_path() {
  const char* value = std::getenv("SRSP_TEST_PLUGIN");
  return value != nullptr ? std::string(value) : std::string();
}

srsp_str make_str(const char* text) {
  srsp_str result{};
  result.data = text;
  result.size = std::strlen(text);
  return result;
}

}  // namespace

TEST_CASE("plugin host rejects calls before load", "[plugin_host]") {
  srsp::core::PluginHost host(make_services());
  srsp_error err{};

  REQUIRE(host.loaded() == false);
  CHECK(host.call(make_str("core.ping"), srsp_bytes{}, nullptr, &err) ==
        SRSP_E_NOT_READY);
  CHECK(err.code == SRSP_E_NOT_READY);
}

TEST_CASE("plugin host reports a missing library", "[plugin_host]") {
  srsp::core::PluginHost host(make_services());
  srsp_error err{};

  CHECK(host.load("this/library/does/not/exist.dll", &err) ==
        SRSP_E_NOT_FOUND);
  CHECK(err.code == SRSP_E_NOT_FOUND);
  CHECK(host.loaded() == false);
}

TEST_CASE("plugin host loads, initialises and invokes the fixture",
          "[plugin_host]") {
  const std::string path = plugin_path();
  if (path.empty()) {
    SKIP("SRSP_TEST_PLUGIN is not set");
  }

  srsp::core::PluginHost host(make_services());
  srsp_error err{};

  REQUIRE(host.load(path, &err) == SRSP_OK);
  CHECK(host.loaded());
  REQUIRE(host.plugin() != nullptr);

  REQUIRE(host.init(srsp_str{nullptr, 0}, nullptr) == SRSP_OK);
  CHECK(host.initialized());

  srsp_buffer response{};
  REQUIRE(host.call(make_str("core.ping"), srsp_bytes{}, &response, &err) ==
          SRSP_OK);
  CHECK(response.data != nullptr);
  CHECK(response.size > 0);
  srsp_host_v1 services = make_services();
  services.free(services.user, response.data);

  CHECK(host.init(srsp_str{nullptr, 0}, &err) == SRSP_E_INVALID_STATE);
}

TEST_CASE("plugin host rejects a second load", "[plugin_host]") {
  const std::string path = plugin_path();
  if (path.empty()) {
    SKIP("SRSP_TEST_PLUGIN is not set");
  }

  srsp::core::PluginHost host(make_services());
  REQUIRE(host.load(path, nullptr) == SRSP_OK);
  CHECK(host.load(path, nullptr) == SRSP_E_INVALID_STATE);
}

TEST_CASE("plugin host checkpoint and restore round-trip", "[plugin_host]") {
  const std::string path = plugin_path();
  if (path.empty()) {
    SKIP("SRSP_TEST_PLUGIN is not set");
  }

  srsp::core::PluginHost host(make_services());
  REQUIRE(host.load(path, nullptr) == SRSP_OK);
  REQUIRE(host.init(srsp_str{nullptr, 0}, nullptr) == SRSP_OK);

  srsp_buffer snapshot{};
  REQUIRE(host.checkpoint(&snapshot, nullptr) == SRSP_OK);
  REQUIRE(snapshot.data != nullptr);

  const srsp_bytes payload{snapshot.data, snapshot.size};
  CHECK(host.restore(payload, nullptr) == SRSP_OK);

  srsp_host_v1 services = make_services();
  services.free(services.user, snapshot.data);
}

TEST_CASE("plugin host stops accepting calls after unload", "[plugin_host]") {
  const std::string path = plugin_path();
  if (path.empty()) {
    SKIP("SRSP_TEST_PLUGIN is not set");
  }

  srsp::core::PluginHost host(make_services());
  REQUIRE(host.load(path, nullptr) == SRSP_OK);
  REQUIRE(host.init(srsp_str{nullptr, 0}, nullptr) == SRSP_OK);

  host.unload();
  CHECK(host.loaded() == false);
  CHECK(host.initialized() == false);

  srsp_error err{};
  CHECK(host.call(make_str("core.ping"), srsp_bytes{}, nullptr, &err) ==
        SRSP_E_NOT_READY);
}
