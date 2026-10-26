/**
 * @file bench_record.cpp
 * @brief Benchmark: MCAP write/read throughput and long-run hash stability.
 *
 * Prints machine-readable lines:
 *   write_mb_s, read_mb_s, messages, ten_minute_ok
 *   hash_steps, hash_mismatches
 *
 * The 10-minute case is evaluated by capacity: a 100 Hz recording of 600 s is
 * 60 000 messages, and the benchmark reports whether the measured throughput
 * sustains that rate with a wide margin. Hash stability is exercised on a real
 * trajectory (several thousand RK4 steps), then replayed and compared.
 */
#include <srsp/dynamics/rigid_body.hpp>
#include <srsp/model/srm.hpp>
#include <srsp/record/mcap.hpp>
#include <srsp/record/state_hash.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kPayloadBytes = 256;
constexpr int kMessages = 60000;  // 100 Hz for 10 minutes
constexpr int kHashSteps = 2000;
constexpr double kDt = 2e-3;

std::string models_dir() {
  const char* value = std::getenv("SRSP_TEST_MODELS");
  return value != nullptr ? std::string(value) : std::string();
}

}  // namespace

int main() {
  const std::string path =
      (std::filesystem::temp_directory_path() / "srsp_bench_record.mcap").string();
  std::vector<std::uint8_t> payload(kPayloadBytes);
  for (std::size_t i = 0; i < payload.size(); ++i) {
    payload[i] = static_cast<std::uint8_t>(i * 31u + 7u);
  }

  const auto write_start = std::chrono::steady_clock::now();
  {
    srsp::record::McapWriter writer;
    if (!writer.open(path, "srsp.bench")) {
      std::fputs("writer open failed\n", stderr);
      return 1;
    }
    const auto schema = writer.add_schema("srsp.state/1", "raw", "{}");
    const auto channel = writer.add_channel(schema, "sim.state", "raw");
    for (int i = 0; i < kMessages; ++i) {
      if (!writer.write_message(channel, static_cast<std::uint64_t>(i),
                                1000ull * static_cast<std::uint64_t>(i), payload.data(),
                                payload.size())) {
        std::fputs("write failed\n", stderr);
        return 1;
      }
    }
    if (!writer.close()) {
      std::fputs("close failed\n", stderr);
      return 1;
    }
  }
  const auto write_stop = std::chrono::steady_clock::now();
  const double write_s =
      std::chrono::duration<double>(write_stop - write_start).count();

  const auto read_start = std::chrono::steady_clock::now();
  srsp::record::McapReader reader;
  if (!reader.open(path)) {
    std::fprintf(stderr, "reader open failed: %s\n", reader.error().c_str());
    return 1;
  }
  std::size_t bytes = 0;
  for (const auto& message : reader.messages()) {
    bytes += message.data.size();
  }
  const auto read_stop = std::chrono::steady_clock::now();
  const double read_s = std::chrono::duration<double>(read_stop - read_start).count();

  if (reader.messages().size() != static_cast<std::size_t>(kMessages)) {
    std::fprintf(stderr, "message count %zu != %d\n", reader.messages().size(), kMessages);
    return 1;
  }
  std::filesystem::remove(path);

  const double write_mb_s = static_cast<double>(bytes) / (1024.0 * 1024.0) / write_s;
  const double read_mb_s = static_cast<double>(bytes) / (1024.0 * 1024.0) / read_s;
  // 10 minutes at 100 Hz = 60 000 messages; require the write to finish in <1 s
  // so recording never becomes the loop bottleneck.
  const bool ten_minute_ok = write_s < 1.0;

  std::printf("messages=%d\n", kMessages);
  std::printf("write_s=%.3f\n", write_s);
  std::printf("read_s=%.3f\n", read_s);
  std::printf("write_mb_s=%.2f\n", write_mb_s);
  std::printf("read_mb_s=%.2f\n", read_mb_s);
  std::printf("ten_minute_ok=%s\n", ten_minute_ok ? "true" : "false");

  if (models_dir().empty()) {
    std::printf("hash_steps=0 hash_mismatches=0 note=SRSP_TEST_MODELS not set\n");
    return 0;
  }
  const auto model = srsp::model::load_from_file(models_dir() + "/arm6.srm.json");
  if (!model.ok()) {
    std::fputs("model load failed\n", stderr);
    return 1;
  }
  const srsp::dynamics::RigidBodyDynamics dynamics(model.model);
  Eigen::VectorXd q = dynamics.neutral_configuration();
  Eigen::VectorXd v = Eigen::VectorXd::Zero(dynamics.nv());
  for (int i = 0; i < dynamics.joint_count(); ++i) {
    v[6 + i] = 0.1;
  }
  const Eigen::VectorXd tau = Eigen::VectorXd::Zero(dynamics.nv());

  std::vector<srsp::record::StateHash> hashes;
  hashes.reserve(kHashSteps);
  for (int step = 0; step < kHashSteps; ++step) {
    dynamics.step(&q, &v, tau, kDt);
    std::vector<double> values;
    values.insert(values.end(), q.data(), q.data() + q.size());
    values.insert(values.end(), v.data(), v.data() + v.size());
    hashes.push_back(srsp::record::hash_state(values));
  }

  // Replay from the same initial state and compare every digest.
  Eigen::VectorXd replay_q = dynamics.neutral_configuration();
  Eigen::VectorXd replay_v = Eigen::VectorXd::Zero(dynamics.nv());
  for (int i = 0; i < dynamics.joint_count(); ++i) {
    replay_v[6 + i] = 0.1;
  }
  int mismatches = 0;
  for (int step = 0; step < kHashSteps; ++step) {
    dynamics.step(&replay_q, &replay_v, tau, kDt);
    std::vector<double> values;
    values.insert(values.end(), replay_q.data(), replay_q.data() + replay_q.size());
    values.insert(values.end(), replay_v.data(), replay_v.data() + replay_v.size());
    if (!(srsp::record::hash_state(values) == hashes[static_cast<std::size_t>(step)])) {
      ++mismatches;
    }
  }
  std::printf("hash_steps=%d hash_mismatches=%d\n", kHashSteps, mismatches);
  return 0;
}
