/**
 * @file sidecar_host.hpp
 * @brief gRPC client for a plugin running in a sidecar process.
 * @brief_zh 面向边车进程中插件的 gRPC 客户端。
 */
#ifndef SRSP_BRIDGE_SIDECAR_HOST_HPP
#define SRSP_BRIDGE_SIDECAR_HOST_HPP

#include <srsp/plugin_api.h>

#include <memory>
#include <string>

namespace grpc {
class Channel;
}

namespace srsp::bridge {

/**
 * @brief Connects to a sidecar and forwards plugin calls over gRPC.
 * @brief_zh 连接边车并通过 gRPC 转发插件调用。
 *
 * The handshake carries a shared token and the host ABI version; the sidecar
 * rejects mismatches. Calls are synchronous with a deadline so a hung sidecar
 * cannot block the kernel forever.
 */
class SidecarHost {
 public:
  /**
   * @param address  "host:port" of the sidecar.
   * @param token    Shared secret established when the sidecar was launched.
   * @param timeout_ms Deadline for each RPC.
   */
  SidecarHost(std::string address, std::string token, int timeout_ms = 2000);
  ~SidecarHost();

  SidecarHost(const SidecarHost&) = delete;
  SidecarHost& operator=(const SidecarHost&) = delete;

  /**
   * @brief Performs the handshake and records the remote plugin identity.
   * @brief_zh 执行握手并记录远端插件标识。
   * @return SRSP_OK, SRSP_E_VERSION_MISMATCH on ABI rejection,
   *         SRSP_E_TIMEOUT when the sidecar cannot be reached.
   */
  srsp_status connect(srsp_error* err);

  /** @brief Invokes a method; response payload is copied into @p out. */
  /** @brief_zh 调用方法；响应载荷拷贝到 out。 */
  srsp_status invoke(const std::string& method, const std::string& payload, std::string* out,
                     srsp_error* err);

  /** @brief Liveness probe; false when the sidecar does not answer. */
  /** @brief_zh 存活探测；边车无响应时返回 false。 */
  bool ping(uint64_t step_index);

  /** @brief Asks the sidecar to shut down; best effort. */
  /** @brief_zh 请求边车关闭；尽力而为。 */
  void shutdown();

  [[nodiscard]] const std::string& plugin_id() const noexcept { return plugin_id_; }
  [[nodiscard]] const std::string& address() const noexcept { return address_; }

 private:
  std::string address_;
  std::string token_;
  int timeout_ms_;
  std::string plugin_id_;
  std::shared_ptr<grpc::Channel> channel_;
};

}  // namespace srsp::bridge

#endif  // SRSP_BRIDGE_SIDECAR_HOST_HPP
