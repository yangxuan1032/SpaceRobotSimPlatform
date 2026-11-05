/**
 * @file test_lockfile.cpp
 * @brief Lock extraction, serialisation and matching (B3).
 */
#include <srsp/record/lockfile.hpp>
#include <srsp/record/run_metadata.hpp>
#include <srsp/record/selection.hpp>

#include <map>
#include <string>

#include <catch2/catch_test_macros.hpp>

using srsp::record::lock_from_metadata;
using srsp::record::lock_matches;
using srsp::record::parse_lock;
using srsp::record::serialize_lock;

TEST_CASE("lock keeps only the reproducibility keys", "[lockfile]") {
  const std::map<std::string, std::string> metadata{
      {"integrator", "semi_implicit"}, {"solver", "composite"},    {"encoding", "raw-f64"},
      {"plugin_lock_hash", "abc"},     {"compiler", "GCC 16.1.0"}, {"build_flags", "-O2"},
      {"schema_hash", "ignored"}};
  const auto lock = lock_from_metadata(metadata);
  CHECK(lock.size() == 6u);
  CHECK(lock.count("schema_hash") == 0u);
}

TEST_CASE("lock round-trips through text", "[lockfile]") {
  const std::map<std::string, std::string> lock{{"integrator", "rk4"}, {"plugin_lock_hash", "abc"}};
  const auto parsed = parse_lock(serialize_lock(lock));
  CHECK(parsed == lock);
}

TEST_CASE("lock comparison reports the first mismatch", "[lockfile]") {
  const std::map<std::string, std::string> recorded{{"integrator", "rk4"},
                                                    {"plugin_lock_hash", "abc"}};
  std::string key;
  CHECK(lock_matches(recorded, recorded, &key));

  const std::map<std::string, std::string> other{{"integrator", "semi_implicit"},
                                                 {"plugin_lock_hash", "abc"}};
  CHECK_FALSE(lock_matches(recorded, other, &key));
  CHECK(key == "integrator");

  const std::map<std::string, std::string> missing{{"plugin_lock_hash", "abc"}, {"solver", "x"}};
  CHECK_FALSE(lock_matches(recorded, missing, &key));
  CHECK(key == "solver");
}
TEST_CASE("plugin version changes the lock hash", "[lockfile]") {
  const auto lock_v1 = srsp::record::canonical_plugin_lock({{"org.srsp.arm", "1.0.0", "", 1}});
  const auto lock_v2 = srsp::record::canonical_plugin_lock({{"org.srsp.arm", "2.0.0", "", 1}});
  CHECK(lock_v1 != lock_v2);
  CHECK(srsp::record::plugin_lock_hash(lock_v1) != srsp::record::plugin_lock_hash(lock_v2));
  // An empty lock must not collide with a real one.
  CHECK(srsp::record::plugin_lock_hash("") != srsp::record::plugin_lock_hash(lock_v1));
}
