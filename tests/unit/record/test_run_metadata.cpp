/**
 * @file test_run_metadata.cpp
 * @brief Plugin lock canonicalisation and replay selection checks (O9/O6).
 */
#include <srsp/record/replay.hpp>
#include <srsp/record/run_metadata.hpp>

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using srsp::record::canonical_plugin_lock;
using srsp::record::check_selection;
using srsp::record::IntegratorTag;
using srsp::record::plugin_lock_hash;
using srsp::record::PluginLockEntry;
using srsp::record::run_metadata_map;
using srsp::record::RunMetadata;
using srsp::record::SelectionMatch;

namespace {

RunMetadata sample() {
  RunMetadata metadata;
  metadata.integrator = IntegratorTag::kSemiImplicit;
  metadata.compiler = "GCC 16.1.0";
  metadata.build_flags = "-O2 -ffp-contract=off";
  metadata.plugin_lock = canonical_plugin_lock({
      {"org.srsp.arm", "1.2.0", "aaaa", 1},
      {"org.srsp.base", "1.0.0", "bbbb", 1},
  });
  return metadata;
}

}  // namespace

TEST_CASE("plugin lock is order independent", "[run_metadata]") {
  const auto a = canonical_plugin_lock({{"b", "1", "x", 1}, {"a", "2", "y", 1}});
  const auto b = canonical_plugin_lock({{"a", "2", "y", 1}, {"b", "1", "x", 1}});
  CHECK(a == b);
  CHECK(plugin_lock_hash(a) == plugin_lock_hash(b));
  CHECK(a.find("a@2#y#1\n") == 0u);
}

TEST_CASE("run metadata carries the reproducibility keys", "[run_metadata]") {
  const auto metadata = run_metadata_map(sample());
  CHECK(metadata.at("integrator") == "semi_implicit");
  CHECK(metadata.at("solver") == "composite");
  CHECK(metadata.at("encoding") == "raw-f64");
  CHECK(metadata.at("compiler") == "GCC 16.1.0");
  CHECK(metadata.at("build_flags") == "-O2 -ffp-contract=off");
  CHECK(metadata.at("plugin_lock_hash").size() == 32u);
}

TEST_CASE("replay selection detects missing and mismatched keys", "[run_metadata]") {
  const auto current = sample();
  const auto recorded = run_metadata_map(current);
  std::string key;
  CHECK(check_selection(recorded, current, &key) == SelectionMatch::kOk);

  auto missing = recorded;
  missing.erase("plugin_lock_hash");
  CHECK(check_selection(missing, current, &key) == SelectionMatch::kMissing);
  CHECK(key == "plugin_lock_hash");

  auto changed = recorded;
  changed["compiler"] = "Clang 22.1.8";
  CHECK(check_selection(changed, current, &key) == SelectionMatch::kMismatch);
  CHECK(key == "compiler");
}
TEST_CASE("replay rejects unknown encodings and missing tags", "[replay]") {
  srsp::record::ChannelInfo good;
  good.topic = "sim.state";
  good.message_encoding = "raw-f64";
  good.metadata = run_metadata_map(sample());

  CHECK(srsp::record::verify_recording({good}).allowed);

  auto unknown = good;
  unknown.message_encoding = "cbor";
  const auto unknown_decision = srsp::record::verify_recording({unknown});
  CHECK_FALSE(unknown_decision.allowed);
  CHECK(unknown_decision.reason.find("unknown encoding") != std::string::npos);

  auto missing = good;
  missing.metadata.erase("solver");
  const auto missing_decision = srsp::record::verify_recording({missing});
  CHECK_FALSE(missing_decision.allowed);
  CHECK(missing_decision.reason.find("solver") != std::string::npos);
}

TEST_CASE("replay policy distinguishes strict from read-only", "[replay]") {
  srsp::record::ChannelInfo channel;
  channel.topic = "sim.state";
  channel.message_encoding = "raw-f64";
  channel.metadata = run_metadata_map(sample());

  const auto current = sample();
  CHECK(srsp::record::verify_against({channel}, current, srsp::record::ReplayPolicy::kStrict)
            .allowed);

  auto other = current;
  other.integrator = IntegratorTag::kRk4;
  const auto strict =
      srsp::record::verify_against({channel}, other, srsp::record::ReplayPolicy::kStrict);
  CHECK_FALSE(strict.allowed);
  CHECK(strict.reason.find("integrator") != std::string::npos);

  const auto lenient =
      srsp::record::verify_against({channel}, other, srsp::record::ReplayPolicy::kAllowReadOnly);
  CHECK(lenient.allowed);
  CHECK(lenient.read_only);
}

TEST_CASE("state hash is stable for identical states", "[replay]") {
  const std::vector<double> state{1.0, 2.5, -3.25, 0.125};
  CHECK(srsp::record::hash_state(state).to_hex() == srsp::record::hash_state(state).to_hex());
  auto changed = state;
  changed[2] += 1e-12;
  CHECK(srsp::record::hash_state(changed).to_hex() != srsp::record::hash_state(state).to_hex());
}
