/**
 * @file scheduler.cpp
 * @brief Implementation of the fixed-step macro-step loop.
 */
#include <srsp/core/scheduler.hpp>

namespace srsp::core {

Scheduler::Scheduler(TimeService& clock, Diagnostics* diagnostics) noexcept
    : clock_(clock), diagnostics_(diagnostics) {}

bool Scheduler::step(const StepCallback& on_step) {
  if (!clock_.advance()) {
    if (diagnostics_ != nullptr) {
      diagnostics_->emit(Severity::kDebug, "scheduler.paused",
                         "macro step skipped because the clock is paused");
    }
    return false;
  }
  ++total_steps_;
  if (on_step) {
    on_step(clock_.step_index(), clock_.now_ns());
  }
  return true;
}

uint64_t Scheduler::run(uint64_t max_steps, const StepCallback& on_step) {
  uint64_t executed = 0;
  while (executed < max_steps && step(on_step)) {
    ++executed;
  }
  return executed;
}

}  // namespace srsp::core
