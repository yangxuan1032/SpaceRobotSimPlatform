/**
 * @file diagnostics.hpp
 * @brief Structured diagnostics with an optional sink and a bounded history.
 */
#ifndef SRSP_CORE_DIAGNOSTICS_HPP
#define SRSP_CORE_DIAGNOSTICS_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace srsp::core {

/** @brief Severity levels shared with the plugin ABI log levels. */
enum class Severity : int32_t {
  kTrace = 0,
  kDebug = 1,
  kInfo = 2,
  kWarn = 3,
  kError = 4,
  kCritical = 5,
};

/** @brief One diagnostic record. */
struct Diagnostic {
  Severity severity = Severity::kInfo;  ///< Severity level.
  std::string code;                     ///< Stable machine-readable code.
  std::string message;                  ///< Human-readable message.
  std::string context_json;             ///< Optional structured context.
};

/**
 * @brief Collects diagnostics and forwards them to a sink.
 *
 * The history is capped by `set_history_capacity()`; the oldest records are
 * dropped first. Diagnostics never throw from `emit()`.
 */
class Diagnostics {
 public:
  using Sink = std::function<void(const Diagnostic&)>;

  /** @brief Creates a collector; @p sink may be empty. */
  explicit Diagnostics(Sink sink = {});

  /** @brief Emits a record, appends it to history and calls the sink. */
  void emit(Severity severity, std::string code, std::string message,
            std::string context_json = {});

  /** @brief Limits the retained history (0 disables retention). */
  void set_history_capacity(std::size_t capacity) noexcept;

  /** @brief Renders a record as a single-line JSON object. */
  [[nodiscard]] static std::string to_json_line(const Diagnostic& record);

  [[nodiscard]] std::size_t count() const noexcept { return records_.size(); }
  [[nodiscard]] std::size_t count(Severity severity) const noexcept;
  [[nodiscard]] const std::vector<Diagnostic>& records() const noexcept {
    return records_;
  }

 private:
  Sink sink_;
  std::vector<Diagnostic> records_;
  std::size_t capacity_ = 1024;
};

}  // namespace srsp::core

#endif  // SRSP_CORE_DIAGNOSTICS_HPP
