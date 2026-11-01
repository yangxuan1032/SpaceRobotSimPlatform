/**
 * @file scheduler.hpp
 * @brief Fixed-step macro-step loop skeleton.
 * @brief_zh 固定步长宏步循环骨架。
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
/** @brief_zh 以宏步为单位推进仿真时钟。 */
class Scheduler {
 public:
  /**
   * @brief Callback invoked once per macro step.
   * @brief_zh 每个宏步调用一次的回调。
   * @param step_index Zero-based index after the clock advanced.
   * @param t_ns Simulation time in TAI nanoseconds.
   */
  using StepCallback = std::function<void(uint64_t step_index, int64_t t_ns)>;

  /**
   * @brief Binds a clock and an optional diagnostics sink.
   * @brief_zh 绑定时钟与可选的诊断接收器。
   * @param clock Clock advanced by each step; must outlive the scheduler.
   * @param diagnostics Optional collector for paused/failure diagnostics.
   */
  Scheduler(TimeService& clock, Diagnostics* diagnostics = nullptr) noexcept;

  /**
   * @brief Executes exactly one macro step.
   * @brief_zh 执行恰好一个宏步。
   * @return False when the clock is paused (a debug diagnostic is emitted).
   */
  bool step(const StepCallback& on_step);

  /**
   * @brief Executes up to @p max_steps macro steps.
   * @brief_zh 最多执行 max_steps 个宏步。
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
