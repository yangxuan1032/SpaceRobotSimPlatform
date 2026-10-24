/**
 * @file test_manifest_load.cpp
 * @brief Unit tests for loading a plugin through its SRSP manifest.
 *
 * The manifest path is provided by ctest through SRSP_TEST_MANIFEST.
 */
#include <srsp/core/plugin_host.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <string>

namespace {

srsp_host_v1 services() {
  srsp_host_v1 table{};
  table.struct_size = sizeof(srsp_host_v1);
  return table;
}

std::string manifest_path() {
  const char* value = std::getenv("SRSP_TEST_MANIFEST");
  return value != nullptr ? std::string(value) : std::string();
}

}  // namespace

TEST_CASE("manifest load records the plugin id", "[plugin_host][manifest]") {
  const std::string path = manifest_path();
  if (path.empty()) {
    SKIP("SRSP_TEST_MANIFEST is not set");
  }

  srsp::core::PluginHost host(services());
  srsp_error err{};
  REQUIRE(host.load_with_manifest(path, &err) == SRSP_OK);
  CHECK(host.loaded());
  CHECK(host.plugin_id() == "org.srsp.examples.hello");
}

TEST_CASE("manifest load rejects a missing file", "[plugin_host][manifest]") {
  srsp::core::PluginHost host(services());
  srsp_error err{};

  CHECK(host.load_with_manifest("no/such/plugin.json", &err) == SRSP_E_NOT_FOUND);
  CHECK(err.code == SRSP_E_NOT_FOUND);
  CHECK_FALSE(host.loaded());
}

TEST_CASE("unload clears the recorded plugin id", "[plugin_host][manifest]") {
  const std::string path = manifest_path();
  if (path.empty()) {
    SKIP("SRSP_TEST_MANIFEST is not set");
  }

  srsp::core::PluginHost host(services());
  REQUIRE(host.load_with_manifest(path, nullptr) == SRSP_OK);
  REQUIRE_FALSE(host.plugin_id().empty());

  host.unload();
  CHECK(host.plugin_id().empty());
}
