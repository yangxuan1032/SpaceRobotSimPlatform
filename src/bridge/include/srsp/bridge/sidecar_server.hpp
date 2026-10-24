/**
 * @file sidecar_server.hpp
 * @brief gRPC server that exposes an in-process plugin as a sidecar.
 */
#ifndef SRSP_BRIDGE_SIDECAR_SERVER_HPP
#define SRSP_BRIDGE_SIDECAR_SERVER_HPP

#include <memory>
#include <string>

namespace srsp::bridge {

/**
 * @brief Serves the PluginRuntime gRPC service backed by a loaded plugin.
 *
 * Lifetime: `start()` loads the plugin and binds the port; `stop()` shuts the
 * plugin down and stops the server. Starting twice is an error.
 */
class SidecarServer {
 public:
  SidecarServer();
  ~SidecarServer();

  SidecarServer(const SidecarServer&) = delete;
  SidecarServer& operator=(const SidecarServer&) = delete;

  /**
   * @brief Loads @p plugin_path and binds @p address (e.g. "127.0.0.1:0").
   * @param address Bind address; port 0 picks an ephemeral port.
   * @param token Shared secret required by the Handshake RPC.
   * @param plugin_path Path of the plugin library to expose.
   * @return True on success; on failure the server stays stopped.
   */
  bool start(const std::string& address, const std::string& token,
             const std::string& plugin_path);

  /** @brief Stops the server and unloads the plugin. */
  void stop();

  /** @brief Bound port after `start()`; 0 when not running. */
  [[nodiscard]] int port() const noexcept { return port_; }

  /** @brief Bound address in "host:port" form; empty when not running. */
  [[nodiscard]] std::string address() const { return address_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  int port_ = 0;
  std::string address_;
};

}  // namespace srsp::bridge

#endif  // SRSP_BRIDGE_SIDECAR_SERVER_HPP
