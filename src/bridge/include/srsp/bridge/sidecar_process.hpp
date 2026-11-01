/**
 * @file sidecar_process.hpp
 * @brief Launches a sidecar child process and reads its readiness line.
 * @brief_zh 启动边车子进程并读取其就绪行。
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
/** @brief_zh 持有一个边车子进程及其标准输出管道。 */
class SidecarProcess {
 public:
  SidecarProcess() = default;
  ~SidecarProcess();

  SidecarProcess(const SidecarProcess&) = delete;
  SidecarProcess& operator=(const SidecarProcess&) = delete;

  /**
   * @brief Starts @p executable with @p args and captures stdout.
   * @brief_zh 以给定参数启动可执行文件并捕获标准输出。
   * @return False when the process could not be created.
   */
  bool start(const std::string& executable, const std::vector<std::string>& args);

  /**
   * @brief Reads one line from the child's stdout.
   * @brief_zh 从子进程标准输出读取一行。
   * @param line Receives the line without the trailing newline.
   * @param timeout_ms Maximum wait.
   * @return False on timeout, EOF or when no process is running.
   */
  bool read_line(std::string* line, int timeout_ms);

  /** @brief Requests termination; safe to call twice. */
  /** @brief_zh 请求终止；可安全调用两次。 */
  void terminate();

  /** @brief Waits for exit and returns the exit code (-1 when unknown). */
  /** @brief_zh 等待退出并返回退出码（未知时为 -1）。 */
  int wait();

  [[nodiscard]] bool running() const noexcept { return pid_ != 0; }

 private:
  bool read_byte(char* out, int timeout_ms);
  void close_handles();

  int pid_ = 0;
#if defined(_WIN32)
  void* process_ = nullptr;    ///< HANDLE / 进程句柄。
  void* pipe_read_ = nullptr;  ///< HANDLE / 读端句柄。
  void* pipe_write_ =
      nullptr;  ///< HANDLE (child side, closed after spawn) / 写端句柄（子进程侧，启动后关闭）。
#else
  int pipe_read_ = -1;
  int pipe_write_ = -1;
#endif
};

}  // namespace srsp::bridge

#endif  // SRSP_BRIDGE_SIDECAR_PROCESS_HPP
