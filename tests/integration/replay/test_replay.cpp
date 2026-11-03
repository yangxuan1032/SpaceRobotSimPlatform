/**
 * @file test_replay.cpp
 * @brief Records a deterministic trajectory to MCAP, verifies the file by
 *        re-hashing its payloads, and re-simulates to compare per-step hashes.
 *
 * Standalone executable (no Catch2) so it can live in the integration layer,
 * which is registered before the unit-test framework is looked up.
 */
#include <srsp/dynamics/rigid_body.hpp>
#include <srsp/model/srm.hpp>
#include <srsp/record/mcap.hpp>
#include <srsp/record/state_hash.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include <cstdio>
#include <cstdlib>

namespace {

using srsp::dynamics::RigidBodyDynamics;
using srsp::record::McapReader;
using srsp::record::McapWriter;
using srsp::record::StateHash;

constexpr int kSteps = 200;
constexpr double kDt = 1e-3;

int fail(const char* what) {
  std::fprintf(stderr, "FAIL: %s\n", what);
  return 1;
}

std::string models_dir() {
  const char* value = std::getenv("SRSP_TEST_MODELS");
  return value != nullptr ? std::string(value) : std::string();
}

std::vector<double> pack(const Eigen::VectorXd& q, const Eigen::VectorXd& v) {
  std::vector<double> values;
  values.reserve(static_cast<std::size_t>(q.size() + v.size()));
  values.insert(values.end(), q.data(), q.data() + q.size());
  values.insert(values.end(), v.data(), v.data() + v.size());
  return values;
}

struct Sample {
  Eigen::VectorXd q;
  Eigen::VectorXd v;
};

std::vector<Sample> simulate(const RigidBodyDynamics& dynamics, const Eigen::VectorXd& q0,
                             const Eigen::VectorXd& v0, const Eigen::VectorXd& tau) {
  Eigen::VectorXd q = q0;
  Eigen::VectorXd v = v0;
  std::vector<Sample> states;
  states.reserve(kSteps);
  for (int step = 0; step < kSteps; ++step) {
    dynamics.step(&q, &v, tau, kDt);
    states.push_back({q, v});
  }
  return states;
}

}  // namespace

int main() {
  if (models_dir().empty()) {
    std::fputs("SKIP: SRSP_TEST_MODELS is not set\n", stderr);
    return 0;
  }
  const auto model = srsp::model::load_from_file(models_dir() + "/arm6.srm.json");
  if (!model.ok()) {
    return fail("model load failed");
  }
  const RigidBodyDynamics dynamics(model.model);

  Eigen::VectorXd q0 = dynamics.neutral_configuration();
  for (int i = 0; i < dynamics.joint_count(); ++i) {
    q0[7 + i] = 0.2 * static_cast<double>(i + 1);
  }
  Eigen::VectorXd v0 = Eigen::VectorXd::Zero(dynamics.nv());
  for (int i = 0; i < dynamics.joint_count(); ++i) {
    v0[6 + i] = 0.15;
  }
  const Eigen::VectorXd tau = Eigen::VectorXd::Zero(dynamics.nv());

  const auto recorded = simulate(dynamics, q0, v0, tau);
  std::vector<StateHash> hashes;
  hashes.reserve(recorded.size());

  const std::string path = (std::filesystem::temp_directory_path() / "srsp_replay.mcap").string();
  {
    McapWriter writer;
    if (!writer.open(path, "srsp.replay")) {
      return fail("writer open failed");
    }
    const auto schema = writer.add_schema("srsp.state/1", "raw-f64", "{}");
    const auto channel = writer.add_channel(schema, "sim.state", "raw-f64");
    for (std::size_t i = 0; i < recorded.size(); ++i) {
      const auto payload = pack(recorded[i].q, recorded[i].v);
      hashes.push_back(srsp::record::hash_state(payload));
      if (!writer.write_message(channel, static_cast<std::uint64_t>(i),
                                1000ull * static_cast<std::uint64_t>(i), payload.data(),
                                payload.size() * sizeof(double))) {
        return fail("write_message failed");
      }
    }
    if (!writer.close() || writer.message_count() != static_cast<std::uint64_t>(kSteps)) {
      return fail("writer close or message count");
    }
  }

  McapReader reader;
  if (!reader.open(path)) {
    std::fprintf(stderr, "FAIL: reader open: %s\n", reader.error().c_str());
    return 1;
  }
  if (reader.profile() != "srsp.replay") {
    return fail("profile mismatch");
  }
  if (reader.messages().size() != hashes.size()) {
    return fail("message count mismatch after read-back");
  }
  for (std::size_t i = 0; i < reader.messages().size(); ++i) {
    const auto& data = reader.messages()[i].data;
    if (srsp::record::hash_bytes(data.data(), data.size()) != hashes[i]) {
      return fail("payload hash mismatch after read-back");
    }
  }

  const auto replayed = simulate(dynamics, q0, v0, tau);
  if (replayed.size() != hashes.size()) {
    return fail("replay length mismatch");
  }
  for (std::size_t i = 0; i < replayed.size(); ++i) {
    if (srsp::record::hash_state(pack(replayed[i].q, replayed[i].v)) != hashes[i]) {
      std::fprintf(stderr, "FAIL: replay hash mismatch at step %zu\n", i);
      return 1;
    }
  }

  std::filesystem::remove(path);
  std::printf("PASS: %d steps recorded, read back and replayed with identical state hashes\n",
              kSteps);
  return 0;
}
