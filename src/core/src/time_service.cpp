/**
 * @file time_service.cpp
 * @brief Implementation of the fixed-step simulation clock.
 */
#include <srsp/core/time_service.hpp>

namespace srsp::core {

TimeService::TimeService(int64_t step_ns, int64_t start_ns) noexcept
    : step_ns_(step_ns > 0 ? step_ns : 1), now_ns_(start_ns) {}

bool TimeService::set_step_ns(int64_t step_ns) noexcept {
  if (step_ns <= 0) {
    return false;
  }
  step_ns_ = step_ns;
  return true;
}

bool TimeService::advance(uint64_t steps) noexcept {
  if (!running_) {
    return false;
  }
  // Integer nanoseconds: accumulation is exact, no floating point drift.
  now_ns_ += step_ns_ * static_cast<int64_t>(steps);
  step_index_ += steps;
  return true;
}

void TimeService::reset(int64_t start_ns) noexcept {
  now_ns_ = start_ns;
  step_index_ = 0;
}

}  // namespace srsp::core
