/**
 * @file main.cpp
 * @brief srsp-sidecar: serves a plugin over the gRPC PluginRuntime service.
 *
 * Usage: srsp-sidecar --port <n> --token <token> --plugin <path>
 * Prints "READY <port>" once the server is bound, then serves until killed.
 */
#include <srsp/bridge/sidecar_server.hpp>

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string option_value(const std::vector<std::string>& args,
                         const std::string& name) {
  for (size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == name) {
      return args[i + 1];
    }
  }
  return {};
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args;
  for (int i = 0; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  const std::string port_text = option_value(args, "--port");
  const std::string token = option_value(args, "--token");
  const std::string plugin = option_value(args, "--plugin");
  if (port_text.empty() || token.empty() || plugin.empty()) {
    std::fputs("usage: srsp-sidecar --port <n> --token <t> --plugin <path>\n",
               stderr);
    return 2;
  }

  srsp::bridge::SidecarServer server;
  if (!server.start("127.0.0.1:" + port_text, token, plugin)) {
    std::fputs("error: sidecar failed to start\n", stderr);
    return 1;
  }

  std::printf("READY %d\n", server.port());
  std::fflush(stdout);

  // Serve until the host terminates the process.
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
