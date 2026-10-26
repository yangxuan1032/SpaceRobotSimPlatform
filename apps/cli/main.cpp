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
#include <srsp/core_api.h>

#if defined(SRSP_HAVE_SIDECAR)
#include <srsp/bridge/sidecar_host.hpp>
#include <srsp/bridge/sidecar_process.hpp>
#endif

#include <cstring>
#include <string>
#include <vector>

#include <cstdio>
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
  if (command == "version") {
    return run_version();
  }
  if (command == "demo") {
    return run_demo(args);
  }
  if (command == "--help" || command == "-h" || command == "help") {
    print_usage();
    return 0;
  }

  std::fprintf(stderr, "error: unknown command '%s'\n", command.c_str());
  print_usage();
  return kExitFailure;
}
