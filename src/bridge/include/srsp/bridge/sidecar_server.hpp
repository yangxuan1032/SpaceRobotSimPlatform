/**
 * @file sidecar_server.hpp
 * @brief gRPC server that exposes an in-process plugin as a sidecar.
 * @brief_zh 将进程内插件暴露为边车的 gRPC 服务端。
 */
#ifndef SRSP_BRIDGE_SIDECAR_SERVER_HPP
#define SRSP_BRIDGE_SIDECAR_SERVER_HPP

#include <memory>
#include <string>

namespace srsp::bridge {

/**
 * @brief Serves the PluginRuntime gRPC service backed by a loaded plugin.
 * @brief_zh 以已加载插件为后端提供 PluginRuntime gRPC 服务。
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
   * @brief_zh 加载插件并绑定地址（例如 127.0.0.1:0）。
   * @param address Bind address; port 0 picks an ephemeral port.
   * @param token Shared secret required by the Handshake RPC.
   * @param plugin_path Path of the plugin library to expose.
   * @return True on success; on failure the server stays stopped.
   */
  bool start(const std::string& address, const std::string& token, const std::string& plugin_path);

  /** @brief Stops the server and unloads the plugin. */
  /** @brief_zh 停止服务并卸载插件。 */
  void stop();

  /** @brief Bound port after `start()`; 0 when not running. */
  /** @brief_zh start() 之后绑定的端口；未运行时为 0。 */
  [[nodiscard]] int port() const noexcept { return port_; }

  /** @brief Bound address in "host:port" form; empty when not running. */
  /** @brief_zh host:port 形式的绑定地址；未运行时为空。 */
  [[nodiscard]] std::string address() const { return address_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  int port_ = 0;
  std::string address_;
};

}  // namespace srsp::bridge

#endif  // SRSP_BRIDGE_SIDECAR_SERVER_HPP
