/**
 * @file sidecar_host.hpp
 * @brief gRPC client for a plugin running in a sidecar process.
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
   * @return SRSP_OK, SRSP_E_VERSION_MISMATCH on ABI rejection,
   *         SRSP_E_TIMEOUT when the sidecar cannot be reached.
   */
  srsp_status connect(srsp_error* err);

  /** @brief Invokes a method; response payload is copied into @p out. */
  srsp_status invoke(const std::string& method, const std::string& payload,
                     std::string* out, srsp_error* err);

  /** @brief Liveness probe; false when the sidecar does not answer. */
  bool ping(uint64_t step_index);

  /** @brief Asks the sidecar to shut down; best effort. */
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
