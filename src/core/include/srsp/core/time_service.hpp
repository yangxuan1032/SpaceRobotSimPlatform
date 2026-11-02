/**
 * @file time_service.hpp
 * @brief Fixed-step simulation clock (TAI nanoseconds since J2000.0).
 * @brief_zh 固定步长仿真时钟（自 J2000.0 起的 TAI 纳秒）。
 *
 * The clock only advances through explicit macro steps; it never reads the
 * wall clock, which keeps simulation results reproducible.
 */
#ifndef SRSP_CORE_TIME_SERVICE_HPP
#define SRSP_CORE_TIME_SERVICE_HPP

#include <cstdint>

namespace srsp::core {

/** @brief Monotonic clock driven by fixed macro steps. */
/** @brief_zh 由固定宏步驱动的单调时钟。 */
/** @brief Where the simulation time comes from. */
enum class TimeSourceKind {
  kFixedStep = 0,  ///< Advance by explicit macro steps (default).
  kRealtime = 1,   ///< Reserved: follow the wall clock.
  kExternal = 2,   ///< Reserved: driven by an external source.
};

class TimeService {
 public:
  /**
   * @brief Creates a clock positioned at @p start_ns.
   * @brief_zh 创建位于 start_ns 的时钟。
   * @param step_ns Macro step in nanoseconds; must be positive.
   * @param start_ns Initial time in TAI nanoseconds.
   */
  explicit TimeService(int64_t step_ns, int64_t start_ns = 0) noexcept;

  /** @brief Changes the macro step; false when @p step_ns is not positive. */
  /** @brief_zh 修改宏步长；step_ns 非正时返回 false。 */
  bool set_step_ns(int64_t step_ns) noexcept;

  /** @brief Advances by @p steps macro steps; false while paused. */
  /** @brief_zh 前进 steps 个宏步；暂停时返回 false。 */
  bool advance(uint64_t steps = 1) noexcept;

  void pause() noexcept { running_ = false; }
  void resume() noexcept { running_ = true; }
  void reset(int64_t start_ns = 0) noexcept;

  [[nodiscard]] int64_t step_ns() const noexcept { return step_ns_; }
  [[nodiscard]] int64_t now_ns() const noexcept { return now_ns_; }
  [[nodiscard]] uint64_t step_index() const noexcept { return step_index_; }
  [[nodiscard]] bool running() const noexcept { return running_; }

  /**
   * @brief Selects the time source; only kFixedStep is implemented.
   * @return False for a reserved kind, leaving the source unchanged.
   */
  bool set_source(TimeSourceKind kind) noexcept {
    if (kind != TimeSourceKind::kFixedStep) {
      return false;
    }
    source_ = kind;
    return true;
  }

  /** @brief Currently selected time source. */
  [[nodiscard]] TimeSourceKind source() const noexcept { return source_; }

 private:
  int64_t step_ns_;
  int64_t now_ns_;
  uint64_t step_index_ = 0;
  bool running_ = true;
  TimeSourceKind source_ = TimeSourceKind::kFixedStep;
};

}  // namespace srsp::core

#endif  // SRSP_CORE_TIME_SERVICE_HPP
