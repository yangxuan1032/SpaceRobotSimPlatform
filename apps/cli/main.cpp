/**
 * @file main.cpp
 * @brief srsp-cli: minimal kernel entry point for M0 verification.
 *
 * Commands:
 *   srsp-cli version
 *   srsp-cli demo --transport inproc --plugin <path> [--method <name>]
 *
 * The sidecar transport is reserved for M0 stage 3 and currently reports
 * SRSP_E_UNSUPPORTED without pretending to work.
 */
#include <srsp/bridge/transport.hpp>
#include <srsp/core/trust.hpp>
#include <srsp/core_api.h>
#include <srsp/record/lockfile.hpp>
#include <srsp/record/mcap.hpp>
#include <srsp/record/replay.hpp>
#include <srsp/record/run_metadata.hpp>

#if defined(SRSP_HAVE_RECORD)
#include <srsp/record/mcap.hpp>

#include <filesystem>
#endif

#if defined(SRSP_HAVE_SOLVER)
#include <srsp/dynamics/rigid_body.hpp>
#include <srsp/model/srm.hpp>
#endif

#if defined(SRSP_HAVE_SIDECAR)
#include <srsp/bridge/sidecar_host.hpp>
#include <srsp/bridge/sidecar_process.hpp>
#endif

#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <cstdio>
#include <fstream>
#include <random>

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace {

/// Exit code for a command that is recognised but not implemented yet.
constexpr int kExitUnsupported = 3;
/// Exit code for a failed plugin operation.
constexpr int kExitFailure = 2;

srsp_str to_str(const std::string& text) {
  srsp_str result{};
  result.data = text.data();
  result.size = text.size();
  return result;
}

void print_usage() {
  std::fputs(
      "usage:\n"
      "  srsp-cli version\n"
      "  srsp-cli soak --plugin <path> [--minutes N] [--sleep-ms N] [--workload ping|step|record] "
      "[--integrator rk4|semi] "
      "[--model <srm>]\n"
      "  srsp-cli demo --transport inproc --plugin <path> [--method <name>]\n",
      stderr);
}

/// @return The value following @p name, or an empty string.
std::string option_value(const std::vector<std::string>& args, const std::string& name) {
  for (size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == name) {
      return args[i + 1];
    }
  }
  return {};
}

int run_version() {
  std::printf("srsp-cli 0.1.0 (ABI %d.%d, plugin api %d)\n", static_cast<int>(SRSP_ABI_MAJOR),
              static_cast<int>(SRSP_ABI_MINOR), static_cast<int>(sizeof(srsp_plugin_v1)));
  return 0;
}

#if defined(SRSP_HAVE_SIDECAR)
/** @brief Random handshake token for one sidecar instance. */
std::string make_token() {
  std::random_device device;
  char buffer[40];
  std::snprintf(buffer, sizeof(buffer), "%08x%08x%08x%08x", device(), device(), device(), device());
  return buffer;
}

/** @brief Launches a sidecar, invokes @p method over gRPC and prints the reply. */
int run_sidecar_demo(const std::vector<std::string>& args) {
  const std::string plugin_path = option_value(args, "--plugin");
  const std::string sidecar_exe = option_value(args, "--sidecar-exe");
  const std::string method =
      option_value(args, "--method").empty() ? "core.ping" : option_value(args, "--method");
  if (plugin_path.empty()) {
    std::fputs("error: --plugin <path> is required for the sidecar transport\n", stderr);
    return kExitFailure;
  }
  if (sidecar_exe.empty()) {
    std::fputs(
        "error: --sidecar-exe <path> is required for the sidecar "
        "transport\n",
        stderr);
    return kExitFailure;
  }

  const std::string token = make_token();
  const std::string sidecar_script = option_value(args, "--sidecar-script");
  const bool restart_enabled = option_value(args, "--no-restart").empty();
  std::vector<std::string> child_args;
  if (!sidecar_script.empty()) {
    child_args.push_back(sidecar_script);
  }
  child_args.insert(child_args.end(), {"--port", "0", "--token", token, "--plugin", plugin_path});

  srsp::bridge::SidecarProcess child;

  // Launches the sidecar and returns its bound address, or an empty string.
  const auto launch = [&]() -> std::string {
    if (!child.start(sidecar_exe, child_args)) {
      return {};
    }
    std::string line;
    if (!child.read_line(&line, 10000) || line.rfind("READY ", 0) != 0) {
      child.terminate();
      child.wait();
      return {};
    }
    return "127.0.0.1:" + line.substr(6);
  };

  std::string address = launch();
  if (address.empty()) {
    std::fprintf(stderr, "error: could not start sidecar '%s'\n", sidecar_exe.c_str());
    return kExitFailure;
  }

  int result = kExitFailure;
  for (int attempt = 0; attempt < 2; ++attempt) {
    srsp::bridge::SidecarHost client(address, token, 5000);
    srsp_error err{};
    if (client.connect(&err) != SRSP_OK) {
      std::fprintf(stderr, "error: sidecar handshake failed: %s\n",
                   err.message != nullptr ? err.message : "unknown");
    } else {
      std::string payload;
      const srsp_status status = client.invoke(method, std::string{}, &payload, &err);
      client.shutdown();
      if (status == SRSP_OK) {
        std::printf("%s\n", payload.c_str());
        result = 0;
        break;
      }
      std::fprintf(stderr, "error: sidecar call '%s' failed: %s\n", method.c_str(),
                   err.message != nullptr ? err.message : "unknown");
    }

    if (!restart_enabled || attempt == 1) {
      break;
    }
    std::fputs("info: restarting the sidecar once\n", stderr);
    child.terminate();
    child.wait();
    address = launch();
    if (address.empty()) {
      std::fputs("error: could not restart the sidecar\n", stderr);
      break;
    }
  }

  child.terminate();
  child.wait();
  return result;
}
#endif  // SRSP_HAVE_SIDECAR

/// Long-running loop used by the soak test: keeps the context and plugin
/// loaded while repeating one call, so leaks show up over time.
int run_soak(const std::vector<std::string>& args) {
  const std::string plugin_path = option_value(args, "--plugin");
  const std::string minutes_text = option_value(args, "--minutes");
  const std::string sleep_text = option_value(args, "--sleep-ms");
  const std::string workload =
      option_value(args, "--workload").empty() ? "ping" : option_value(args, "--workload");
  const std::string trust_name =
      option_value(args, "--trust").empty() ? "managed" : option_value(args, "--trust");
  srsp::core::TrustLevel trust_level = srsp::core::TrustLevel::kManaged;
  if (trust_name == "builtin") {
    trust_level = srsp::core::TrustLevel::kBuiltin;
  } else if (trust_name == "managed") {
    trust_level = srsp::core::TrustLevel::kManaged;
  } else if (trust_name == "isolated") {
    trust_level = srsp::core::TrustLevel::kIsolated;
  } else if (trust_name != "development") {
    std::fprintf(stderr, "error: unknown trust level '%s'\n", trust_name.c_str());
    return kExitFailure;
  } else {
    trust_level = srsp::core::TrustLevel::kDevelopment;
  }
  if (!srsp::core::allows_in_process(trust_level)) {
    std::fprintf(stderr, "error: trust level '%s' is sidecar-only; use --transport sidecar\n",
                 trust_name.c_str());
    return kExitUnsupported;
  }

  const std::string integrator_name =
      option_value(args, "--integrator").empty() ? "rk4" : option_value(args, "--integrator");
  if (integrator_name != "rk4" && integrator_name != "semi") {
    std::fprintf(stderr, "error: unknown integrator '%s' (rk4|semi)\n", integrator_name.c_str());
    return kExitFailure;
  }
  const srsp::dynamics::IntegratorKind integrator_kind =
      integrator_name == "semi" ? srsp::dynamics::IntegratorKind::kSemiImplicit
                                : srsp::dynamics::IntegratorKind::kRk4;
  const std::string model_path = option_value(args, "--model");
  const double minutes = minutes_text.empty() ? 1.0 : std::stod(minutes_text);
  const int sleep_ms = sleep_text.empty() ? 5 : std::stoi(sleep_text);

  if (plugin_path.empty()) {
    std::fputs("error: --plugin <path> is required for soak\n", stderr);
    return kExitFailure;
  }
  if (minutes <= 0.0) {
    std::fputs("error: --minutes must be positive\n", stderr);
    return kExitFailure;
  }

  srsp_error err{};
  srsp_context_config config{};
  config.struct_size = sizeof(srsp_context_config);
  config.flags = SRSP_CTX_FLAG_HEADLESS;
  // The selection lives in the one runtime configuration entry (docs/02 15.1).
  config.integrator = integrator_name == "semi" ? 1u : 0u;

  srsp_context* ctx = nullptr;
  if (srsp_context_create(&config, &ctx, &err) != SRSP_OK) {
    std::fprintf(stderr, "error: context create failed: %s\n",
                 err.message != nullptr ? err.message : "unknown");
    return kExitFailure;
  }
  if (srsp_load_plugin(ctx, to_str(plugin_path), &err) != SRSP_OK) {
    std::fprintf(stderr, "error: load failed: %s\n",
                 err.message != nullptr ? err.message : "unknown");
    srsp_context_destroy(ctx);
    return kExitFailure;
  }

  // Identity of the loaded plugin, used to build the replay lock (B3).
  std::string plugin_identity;
  {
    srsp_buffer identity{};
    if (srsp_plugin_identity(ctx, &identity, &err) == SRSP_OK) {
      plugin_identity.assign(reinterpret_cast<const char*>(identity.data), identity.size);
      srsp_buffer_release(ctx, &identity);
    }
  }
  std::string plugin_lock_text;
  {
    const std::size_t first = plugin_identity.find('|');
    const std::size_t second =
        first == std::string::npos ? std::string::npos : plugin_identity.find('|', first + 1);
    if (first != std::string::npos && second != std::string::npos) {
      srsp::record::PluginLockEntry entry;
      entry.id = plugin_identity.substr(0, first);
      entry.version = plugin_identity.substr(first + 1, second - first - 1);
      entry.api_major = static_cast<std::uint32_t>(std::stoul(plugin_identity.substr(second + 1)));
      plugin_lock_text = srsp::record::canonical_plugin_lock({entry});
    }
  }

  if (workload == "step") {
#if defined(SRSP_HAVE_RECORD)
#include <srsp/record/mcap.hpp>

#include <filesystem>
#endif

#if defined(SRSP_HAVE_SOLVER)
    const std::string path = model_path.empty() ? "examples/models/arm6.srm.json" : model_path;
    const auto loaded = srsp::model::load_from_file(path);
    if (!loaded.ok()) {
      std::fprintf(stderr, "error: cannot load model %s\n", path.c_str());
      srsp_unload_plugin(ctx, nullptr);
      srsp_context_destroy(ctx);
      return kExitFailure;
    }
    srsp::dynamics::RigidBodyDynamics dynamics(loaded.model);
    dynamics.set_integrator(integrator_kind);
    Eigen::VectorXd q = dynamics.neutral_configuration();
    Eigen::VectorXd v = Eigen::VectorXd::Zero(dynamics.nv());
    const Eigen::VectorXd tau = Eigen::VectorXd::Zero(dynamics.nv());
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::duration<double>(minutes * 60.0);
    std::uint64_t steps = 0;
    auto next_report = start + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < deadline) {
      dynamics.step(&q, &v, tau, 1e-3);
      ++steps;
      if (sleep_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
      }
      const auto now = std::chrono::steady_clock::now();
      if (now >= next_report) {
        const double elapsed = std::chrono::duration<double>(now - start).count();
        std::printf("SOAK elapsed=%.1fs steps=%llu\n", elapsed,
                    static_cast<unsigned long long>(steps));
        std::fflush(stdout);
        next_report = now + std::chrono::seconds(30);
      }
    }
    srsp_unload_plugin(ctx, nullptr);
    srsp_context_destroy(ctx);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("SUMMARY elapsed=%.1fs steps=%llu failures=0\n", elapsed,
                static_cast<unsigned long long>(steps));
    return 0;
#else
    std::fputs("error: this build has no solver support (--workload step)\n", stderr);
    srsp_unload_plugin(ctx, nullptr);
    srsp_context_destroy(ctx);
    return kExitUnsupported;
#endif
  }

  if (workload == "record") {
#if defined(SRSP_HAVE_RECORD)
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "srsp_soak_record.mcap";
    std::vector<std::uint8_t> payload(256, 0x5A);
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::duration<double>(minutes * 60.0);
    std::uint64_t files = 0;
    auto next_report = start + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < deadline) {
      srsp::record::McapWriter writer;
      if (writer.open(path.string(), "srsp.soak")) {
        const auto schema = writer.add_schema("srsp.state/1", "raw-f64", "{}");
        srsp::record::RunMetadata run;
        run.integrator = integrator_kind == srsp::dynamics::IntegratorKind::kSemiImplicit
                             ? srsp::record::IntegratorTag::kSemiImplicit
                             : srsp::record::IntegratorTag::kRk4;
        run.compiler = SRSP_BUILD_COMPILER;
        run.build_flags = SRSP_BUILD_FLAGS;
        run.plugin_lock = plugin_lock_text;
        std::map<std::string, std::string> metadata{{"schema_hash", "soak"}, {"abi", "1"}};
        for (const auto& [key, value] : srsp::record::run_metadata_map(run)) {
          metadata[key] = value;
        }
        const auto channel = writer.add_channel(schema, "sim.state", "raw-f64", metadata);
        for (int i = 0; i < 32; ++i) {
          writer.write_message(channel, static_cast<std::uint64_t>(i),
                               1000ull * static_cast<std::uint64_t>(i), payload.data(),
                               payload.size());
        }
        writer.close();
        ++files;
      }
      if (std::getenv("SRSP_KEEP_MCAP") == nullptr) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
      }
      if (sleep_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
      }
      const auto now = std::chrono::steady_clock::now();
      if (now >= next_report) {
        const double elapsed = std::chrono::duration<double>(now - start).count();
        std::printf("SOAK elapsed=%.1fs recordings=%llu\n", elapsed,
                    static_cast<unsigned long long>(files));
        std::fflush(stdout);
        next_report = now + std::chrono::seconds(30);
      }
    }
    srsp_unload_plugin(ctx, nullptr);
    srsp_context_destroy(ctx);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("SUMMARY elapsed=%.1fs recordings=%llu failures=0\n", elapsed,
                static_cast<unsigned long long>(files));
    return 0;
#else
    std::fputs("error: this build has no recording support (--workload record)\n", stderr);
    srsp_unload_plugin(ctx, nullptr);
    srsp_context_destroy(ctx);
    return kExitUnsupported;
#endif
  }

  const auto start = std::chrono::steady_clock::now();
  const auto deadline = start + std::chrono::duration<double>(minutes * 60.0);
  std::uint64_t iterations = 0;
  std::uint64_t failures = 0;
  auto next_report = start + std::chrono::seconds(30);

  while (std::chrono::steady_clock::now() < deadline) {
    srsp_buffer response{};
    if (srsp_plugin_call(ctx, srsp_str{}, to_str("core.ping"), srsp_bytes{}, &response, &err) ==
        SRSP_OK) {
      srsp_buffer_release(ctx, &response);
    } else {
      ++failures;
    }
    ++iterations;
    if (sleep_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
    const auto now = std::chrono::steady_clock::now();
    if (now >= next_report) {
      const double elapsed = std::chrono::duration<double>(now - start).count();
      std::printf("SOAK elapsed=%.1fs iterations=%llu failures=%llu\n", elapsed,
                  static_cast<unsigned long long>(iterations),
                  static_cast<unsigned long long>(failures));
      std::fflush(stdout);
      next_report = now + std::chrono::seconds(30);
    }
  }

  srsp_unload_plugin(ctx, nullptr);
  srsp_context_destroy(ctx);

  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::printf("SUMMARY elapsed=%.1fs iterations=%llu failures=%llu\n", elapsed,
              static_cast<unsigned long long>(iterations),
              static_cast<unsigned long long>(failures));
  return failures == 0 ? 0 : kExitFailure;
}
int run_demo(const std::vector<std::string>& args) {
  const std::string transport = option_value(args, "--transport");
  const std::string plugin_path = option_value(args, "--plugin");
  const std::string method =
      option_value(args, "--method").empty() ? "core.ping" : option_value(args, "--method");

  if (transport.empty()) {
    std::fputs("error: --transport is required (inproc|sidecar)\n", stderr);
    return kExitFailure;
  }
  if (transport == "sidecar") {
#if defined(SRSP_HAVE_SIDECAR)
    return run_sidecar_demo(args);
#else
    std::fputs("error: this build does not include the gRPC sidecar bridge\n", stderr);
    return kExitUnsupported;
#endif
  }
  if (transport != "inproc") {
    std::fprintf(stderr, "error: unknown transport '%s'\n", transport.c_str());
    return kExitFailure;
  }
  if (plugin_path.empty()) {
    std::fputs("error: --plugin <path> is required for the inproc transport\n", stderr);
    return kExitFailure;
  }

  // The inproc path goes through the host-side C API on purpose: it is the
  // only consumer that proves core_api.h stays usable from a pure C surface.
  srsp_error err{};
  srsp_context_config config{};
  config.struct_size = sizeof(srsp_context_config);
  config.log = nullptr;
  config.log_user = nullptr;
  config.flags = SRSP_CTX_FLAG_HEADLESS;

  srsp_context* ctx = nullptr;
  if (srsp_context_create(&config, &ctx, &err) != SRSP_OK) {
    std::fprintf(stderr, "error: context create failed: %s\n",
                 err.message != nullptr ? err.message : "unknown");
    return kExitFailure;
  }

  int result = kExitFailure;
  if (srsp_load_plugin(ctx, to_str(plugin_path), &err) != SRSP_OK) {
    std::fprintf(stderr, "error: load failed: %s\n",
                 err.message != nullptr ? err.message : "unknown");
  } else {
    srsp_buffer response{};
    const srsp_status status =
        srsp_plugin_call(ctx, srsp_str{}, to_str(method), srsp_bytes{}, &response, &err);
    if (status == SRSP_OK) {
      const char* text =
          response.data != nullptr ? reinterpret_cast<const char*>(response.data) : "";
      std::printf("%.*s\n", static_cast<int>(response.size), text);
      srsp_buffer_release(ctx, &response);
      result = 0;
    } else {
      std::fprintf(stderr, "error: call '%s' failed: %s\n", method.c_str(),
                   err.message != nullptr ? err.message : "unknown");
    }
    srsp_unload_plugin(ctx, nullptr);
  }

  srsp_context_destroy(ctx);
  return result;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args;
  args.reserve(static_cast<size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  if (args.size() < 2) {
    print_usage();
    return kExitFailure;
  }

  const std::string& command = args[1];

  // Trust is a global selection: a sidecar-only plugin must not reach an
  // in-process path, whatever command was given (central trust check).
  {
    const std::string trust_arg = option_value(args, "--trust");
    if (trust_arg == "isolated" || trust_arg == "development") {
      std::fprintf(stderr, "error: trust level '%s' is sidecar-only; use --transport sidecar\n",
                   trust_arg.c_str());
      return kExitUnsupported;
    }
    if (!trust_arg.empty() && trust_arg != "builtin" && trust_arg != "managed") {
      std::fprintf(stderr, "error: unknown trust level '%s'\n", trust_arg.c_str());
      return kExitFailure;
    }
  }
  if (command == "lock") {
    if (args.size() < 3) {
      std::fprintf(stderr, "error: lock needs a recording path\n");
      return kExitFailure;
    }
    srsp::record::McapReader reader;
    if (!reader.open(args[2]) || reader.channels().empty()) {
      std::fprintf(stderr, "error: cannot read recording '%s'\n", args[1].c_str());
      return kExitFailure;
    }
    const auto lock = srsp::record::lock_from_metadata(reader.channels().front().metadata);
    const std::string out =
        option_value(args, "--out").empty() ? args[1] + ".lock" : option_value(args, "--out");
    std::ofstream file(out, std::ios::trunc);
    if (!file) {
      std::fprintf(stderr, "error: cannot write '%s'\n", out.c_str());
      return kExitFailure;
    }
    file << srsp::record::serialize_lock(lock);
    std::printf("lock written=%s keys=%zu\n", out.c_str(), lock.size());
    return 0;
  }

  if (command == "replay-check") {
    if (args.size() < 3) {
      std::fprintf(stderr, "error: replay-check needs a recording path\n");
      return kExitFailure;
    }
    srsp::record::McapReader reader;
    if (!reader.open(args[2])) {
      std::fprintf(stderr, "error: cannot open '%s'\n", args[1].c_str());
      return kExitFailure;
    }
    const auto decision = srsp::record::verify_recording(reader.channels());
    if (!decision.allowed) {
      std::printf("replay allowed=no reason=%s\n", decision.reason.c_str());
      return kExitFailure;
    }
    const std::string lock_path = option_value(args, "--lock");
    if (lock_path.empty()) {
      std::printf("replay allowed=yes lock=unchecked\n");
      return 0;
    }
    std::ifstream file(lock_path);
    if (!file) {
      std::fprintf(stderr, "error: cannot read lock '%s'\n", lock_path.c_str());
      return kExitFailure;
    }
    const std::string text((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    const auto expected = srsp::record::parse_lock(text);
    const auto recorded = srsp::record::lock_from_metadata(reader.channels().front().metadata);
    std::string key;
    if (!srsp::record::lock_matches(recorded, expected, &key)) {
      std::printf("replay allowed=no key=%s recorded=%s expected=%s\n", key.c_str(),
                  recorded.count(key) != 0u ? recorded.at(key).c_str() : "<none>",
                  expected.at(key).c_str());
      return kExitFailure;
    }
    std::printf("replay allowed=yes lock=matched keys=%zu\n", expected.size());
    return 0;
  }

  if (command == "inspect") {
    if (args.size() < 3) {
      std::fprintf(stderr, "error: inspect needs a recording path\n");
      return kExitFailure;
    }
    srsp::record::McapReader reader;
    if (!reader.open(args[2])) {
      std::fprintf(stderr, "error: cannot open '%s'\n", args[1].c_str());
      return kExitFailure;
    }
    const auto decision = srsp::record::verify_recording(reader.channels());
    std::printf("channels=%zu\n", reader.channels().size());
    for (const auto& channel : reader.channels()) {
      std::printf("channel topic=%s encoding=%s\n", channel.topic.c_str(),
                  channel.message_encoding.c_str());
      for (const auto& [key, value] : channel.metadata) {
        std::printf("  metadata %s=%s\n", key.c_str(), value.c_str());
      }
    }
    std::printf("replay allowed=%s read_only=%s reason=%s\n", decision.allowed ? "yes" : "no",
                decision.read_only ? "yes" : "no",
                decision.reason.empty() ? "<none>" : decision.reason.c_str());
    return decision.allowed ? 0 : kExitFailure;
  }

  if (command == "version") {
    return run_version();
  }
  if (command == "demo") {
    return run_demo(args);
  }
  if (command == "soak") {
    return run_soak(args);
  }
  if (command == "--help" || command == "-h" || command == "help") {
    print_usage();
    return 0;
  }

  std::fprintf(stderr, "error: unknown command '%s'\n", command.c_str());
  print_usage();
  return kExitFailure;
}
