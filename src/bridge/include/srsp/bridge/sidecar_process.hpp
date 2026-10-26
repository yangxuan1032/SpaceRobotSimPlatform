/**
 * @file sidecar_process.hpp
 * @brief Launches a sidecar child process and reads its readiness line.
 *
 * The child prints "READY <port>" on stdout once the gRPC server is bound, so
 * the host can use an ephemeral port without guessing a free one.
 */
#ifndef SRSP_BRIDGE_SIDECAR_PROCESS_HPP
#define SRSP_BRIDGE_SIDECAR_PROCESS_HPP

#include <string>
#include <vector>

namespace srsp::bridge {

/** @brief Owns one sidecar child process and its stdout pipe. */
class SidecarProcess {
 public:
  SidecarProcess() = default;
  ~SidecarProcess();

  SidecarProcess(const SidecarProcess&) = delete;
  SidecarProcess& operator=(const SidecarProcess&) = delete;

  /**
   * @brief Starts @p executable with @p args and captures stdout.
   * @return False when the process could not be created.
   */
  bool start(const std::string& executable, const std::vector<std::string>& args);

  /**
   * @brief Reads one line from the child's stdout.
   * @param line Receives the line without the trailing newline.
   * @param timeout_ms Maximum wait.
   * @return False on timeout, EOF or when no process is running.
   */
  bool read_line(std::string* line, int timeout_ms);

  /** @brief Requests termination; safe to call twice. */
  void terminate();

  /** @brief Waits for exit and returns the exit code (-1 when unknown). */
  int wait();

  [[nodiscard]] bool running() const noexcept { return pid_ != 0; }

 private:
  bool read_byte(char* out, int timeout_ms);
  void close_handles();

  int pid_ = 0;
#if defined(_WIN32)
  void* process_ = nullptr;     ///< HANDLE
  void* pipe_read_ = nullptr;   ///< HANDLE
  void* pipe_write_ = nullptr;  ///< HANDLE (child side, closed after spawn)
#else
  int pipe_read_ = -1;
  int pipe_write_ = -1;
#endif
};

}  // namespace srsp::bridge

#endif  // SRSP_BRIDGE_SIDECAR_PROCESS_HPP
