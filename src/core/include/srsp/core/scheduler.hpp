/**
 * @file scheduler.hpp
 * @brief Fixed-step macro-step loop skeleton.
 *
 * Sensor sampling, controller updates, force accumulation and dynamics are
 * registered by callers as the step callback; this class only owns the loop
 * and the clock discipline.
 */
#ifndef SRSP_CORE_SCHEDULER_HPP
#define SRSP_CORE_SCHEDULER_HPP

#include <srsp/core/diagnostics.hpp>
#include <srsp/core/time_service.hpp>

#include <cstdint>
#include <functional>

namespace srsp::core {

/** @brief Drives the simulation clock one macro step at a time. */
class Scheduler {
 public:
  /**
   * @brief Callback invoked once per macro step.
   * @param step_index Zero-based index after the clock advanced.
   * @param t_ns Simulation time in TAI nanoseconds.
   */
  using StepCallback = std::function<void(uint64_t step_index, int64_t t_ns)>;

  /**
   * @brief Binds a clock and an optional diagnostics sink.
   * @param clock Clock advanced by each step; must outlive the scheduler.
   * @param diagnostics Optional collector for paused/failure diagnostics.
   */
  Scheduler(TimeService& clock, Diagnostics* diagnostics = nullptr) noexcept;

  /**
   * @brief Executes exactly one macro step.
   * @return False when the clock is paused (a debug diagnostic is emitted).
   */
  bool step(const StepCallback& on_step);

  /**
   * @brief Executes up to @p max_steps macro steps.
   * @return Number of steps actually executed; stops early when paused.
   */
  uint64_t run(uint64_t max_steps, const StepCallback& on_step);

  [[nodiscard]] uint64_t total_steps() const noexcept { return total_steps_; }

 private:
  TimeService& clock_;
  Diagnostics* diagnostics_ = nullptr;
  uint64_t total_steps_ = 0;
};

}  // namespace srsp::core

#endif  // SRSP_CORE_SCHEDULER_HPP
