/**
 * @file diagnostics.hpp
 * @brief Structured diagnostics with an optional sink and a bounded history.
 * @brief_zh 结构化诊断：可选接收器与有界历史。
 */
#ifndef SRSP_CORE_DIAGNOSTICS_HPP
#define SRSP_CORE_DIAGNOSTICS_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <cstddef>

namespace srsp::core {

/** @brief Severity levels shared with the plugin ABI log levels. */
/** @brief_zh 与插件 ABI 日志级别共享的严重程度。 */
enum class Severity : int32_t {
  kTrace = 0,
  kDebug = 1,
  kInfo = 2,
  kWarn = 3,
  kError = 4,
  kCritical = 5,
};

/** @brief One diagnostic record. */
/** @brief_zh 一条诊断记录。 */
struct Diagnostic {
  Severity severity = Severity::kInfo;  ///< Severity level. / 严重程度。
  std::string code;                     ///< Stable machine-readable code. / 稳定的机器可读码。
  std::string message;                  ///< Human-readable message. / 人类可读消息。
  std::string context_json;             ///< Optional structured context. / 可选的结构化上下文。
};

/**
 * @brief Collects diagnostics and forwards them to a sink.
 * @brief_zh 收集诊断并转发给接收器。
 *
 * The history is capped by `set_history_capacity()`; the oldest records are
 * dropped first. Diagnostics never throw from `emit()`.
 */
class Diagnostics {
 public:
  using Sink = std::function<void(const Diagnostic&)>;

  /** @brief Creates a collector; @p sink may be empty. */
  /** @brief_zh 创建收集器；sink 可为空。 */
  explicit Diagnostics(Sink sink = {});

  /** @brief Emits a record, appends it to history and calls the sink. */
  /** @brief_zh 发出一条记录，追加到历史并调用接收器。 */
  void emit(Severity severity, std::string code, std::string message,
            std::string context_json = {});

  /** @brief Limits the retained history (0 disables retention). */
  /** @brief_zh 限制保留的历史条数（0 表示不保留）。 */
  void set_history_capacity(std::size_t capacity) noexcept;

  /** @brief Renders a record as a single-line JSON object. */
  /** @brief_zh 将记录渲染为单行 JSON 对象。 */
  [[nodiscard]] static std::string to_json_line(const Diagnostic& record);

  [[nodiscard]] std::size_t count() const noexcept { return records_.size(); }
  [[nodiscard]] std::size_t count(Severity severity) const noexcept;
  [[nodiscard]] const std::vector<Diagnostic>& records() const noexcept { return records_; }

 private:
  Sink sink_;
  std::vector<Diagnostic> records_;
  std::size_t capacity_ = 1024;
};

}  // namespace srsp::core

#endif  // SRSP_CORE_DIAGNOSTICS_HPP
