/**
 * @file test_registry.cpp
 * @brief Unit tests for srsp::core::Registry.
 */
#include <srsp/core/registry.hpp>

#include <catch2/catch_test_macros.hpp>

using srsp::core::CapabilityRecord;
using srsp::core::PluginRecord;
using srsp::core::Registry;

namespace {

PluginRecord make_plugin(const char* id) {
  PluginRecord record;
  record.id = id;
  record.version = "1.0.0";
  record.library_path = "plugins/" + record.id + ".dll";
  return record;
}

CapabilityRecord make_capability(const char* name, const char* plugin, int priority) {
  CapabilityRecord record;
  record.name = name;
  record.major = 1;
  record.plugin_id = plugin;
  record.priority = priority;
  return record;
}

}  // namespace

TEST_CASE("registry rejects duplicate plugin ids", "[registry]") {
  Registry registry;
  CHECK(registry.register_plugin(make_plugin("org.srsp.a")));
  CHECK_FALSE(registry.register_plugin(make_plugin("org.srsp.a")));
  CHECK(registry.plugins().size() == 1);
}

TEST_CASE("registry resolves the highest priority implementation", "[registry]") {
  Registry registry;
  REQUIRE(registry.register_plugin(make_plugin("org.srsp.rigid")));
  REQUIRE(registry.register_plugin(make_plugin("org.srsp.alt")));
  REQUIRE(registry.add_capability(
      make_capability("srsp.dynamics/solver", "org.srsp.rigid", 10)));
  REQUIRE(registry.add_capability(
      make_capability("srsp.dynamics/solver", "org.srsp.alt", 50)));

  const CapabilityRecord* best =
      registry.resolve("srsp.dynamics/solver", 1);
  REQUIRE(best != nullptr);
  CHECK(best->plugin_id == "org.srsp.alt");
  CHECK(best->priority == 50);
}

TEST_CASE("registry keeps the earliest entry on equal priority", "[registry]") {
  Registry registry;
  REQUIRE(registry.register_plugin(make_plugin("org.srsp.first")));
  REQUIRE(registry.register_plugin(make_plugin("org.srsp.second")));
  REQUIRE(registry.add_capability(
      make_capability("srsp.dynamics/solver", "org.srsp.first", 5)));
  REQUIRE(registry.add_capability(
      make_capability("srsp.dynamics/solver", "org.srsp.second", 5)));

  const CapabilityRecord* best = registry.resolve("srsp.dynamics/solver", 1);
  REQUIRE(best != nullptr);
  CHECK(best->plugin_id == "org.srsp.first");
}

TEST_CASE("registry ignores a different major version", "[registry]") {
  Registry registry;
  REQUIRE(registry.register_plugin(make_plugin("org.srsp.rigid")));
  REQUIRE(registry.add_capability(
      make_capability("srsp.dynamics/solver", "org.srsp.rigid", 1)));

  CHECK(registry.resolve("srsp.dynamics/solver", 2) == nullptr);
  CHECK(registry.resolve("srsp.unknown/capability", 1) == nullptr);
}

TEST_CASE("registry rejects capabilities for unknown plugins", "[registry]") {
  Registry registry;
  CHECK_FALSE(registry.add_capability(
      make_capability("srsp.dynamics/solver", "org.srsp.missing", 1)));
  CHECK(registry.capabilities().empty());
}

TEST_CASE("unregister removes the plugin and its capabilities", "[registry]") {
  Registry registry;
  REQUIRE(registry.register_plugin(make_plugin("org.srsp.rigid")));
  REQUIRE(registry.add_capability(
      make_capability("srsp.dynamics/solver", "org.srsp.rigid", 1)));

  CHECK(registry.unregister_plugin("org.srsp.rigid"));
  CHECK(registry.find_plugin("org.srsp.rigid") == nullptr);
  CHECK(registry.capabilities().empty());
  CHECK_FALSE(registry.unregister_plugin("org.srsp.rigid"));
}

TEST_CASE("registry tracks the loaded flag", "[registry]") {
  Registry registry;
  REQUIRE(registry.register_plugin(make_plugin("org.srsp.rigid")));
  REQUIRE(registry.mark_loaded("org.srsp.rigid", true));

  const PluginRecord* record = registry.find_plugin("org.srsp.rigid");
  REQUIRE(record != nullptr);
  CHECK(record->loaded);
  CHECK_FALSE(registry.mark_loaded("org.srsp.missing", true));
}
