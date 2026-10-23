/**
 * @file time_service.hpp
 * @brief Fixed-step simulation clock (TAI nanoseconds since J2000.0).
 *
 * The clock only advances through explicit macro steps; it never reads the
 * wall clock, which keeps simulation results reproducible.
 */
#ifndef SRSP_CORE_TIME_SERVICE_HPP
#define SRSP_CORE_TIME_SERVICE_HPP

#include <cstdint>

namespace srsp::core {

/** @brief Monotonic clock driven by fixed macro steps. */
class TimeService {
 public:
  /**
   * @brief Creates a clock positioned at @p start_ns.
   * @param step_ns Macro step in nanoseconds; must be positive.
   * @param start_ns Initial time in TAI nanoseconds.
   */
  explicit TimeService(int64_t step_ns, int64_t start_ns = 0) noexcept;

  /** @brief Changes the macro step; false when @p step_ns is not positive. */
  bool set_step_ns(int64_t step_ns) noexcept;

  /** @brief Advances by @p steps macro steps; false while paused. */
  bool advance(uint64_t steps = 1) noexcept;

  void pause() noexcept { running_ = false; }
  void resume() noexcept { running_ = true; }
  void reset(int64_t start_ns = 0) noexcept;

  [[nodiscard]] int64_t step_ns() const noexcept { return step_ns_; }
  [[nodiscard]] int64_t now_ns() const noexcept { return now_ns_; }
  [[nodiscard]] uint64_t step_index() const noexcept { return step_index_; }
  [[nodiscard]] bool running() const noexcept { return running_; }

 private:
  int64_t step_ns_;
  int64_t now_ns_;
  uint64_t step_index_ = 0;
  bool running_ = true;
};

}  // namespace srsp::core

#endif  // SRSP_CORE_TIME_SERVICE_HPP
