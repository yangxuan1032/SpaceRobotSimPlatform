/**
 * @file fault_policy.hpp
 * @brief What the kernel does when a plugin call fails (B4).
 *
 * Kept as a pure decision function so the scheduler can adopt it without
 * changing the plugin ABI; the policy is not wired into dispatch yet.
 */
#ifndef SRSP_CORE_FAULT_POLICY_HPP
#define SRSP_CORE_FAULT_POLICY_HPP

#include <cstdint>
#include <string_view>

namespace srsp::core {

/** @brief Configured reaction to a failing plugin. */
enum class FaultPolicy : std::uint8_t {
  kStop = 0,     ///< Abort the run on the first failure.
  kDisable = 1,  ///< Disable the plugin and keep the rest running.
  kRetry = 2,    ///< Retry up to a limit, then disable.
};

/** @brief Action the kernel should take now. */
enum class FaultAction : std::uint8_t {
  kStop = 0,
  kDisable = 1,
  kRetry = 2,
};

/**
 * @brief Decides the action after @p consecutive_failures failures.
 * @param policy Configured policy.
 * @param consecutive_failures Failures since the last success (>= 1).
 * @param max_retries Retry budget for kRetry.
 */
[[nodiscard]] constexpr FaultAction decide_fault(FaultPolicy policy,
                                                 std::uint32_t consecutive_failures,
                                                 std::uint32_t max_retries) noexcept {
  if (consecutive_failures == 0u) {
    return FaultAction::kRetry;
  }
  switch (policy) {
    case FaultPolicy::kStop:
      return FaultAction::kStop;
    case FaultPolicy::kDisable:
      return FaultAction::kDisable;
    case FaultPolicy::kRetry:
      return consecutive_failures <= max_retries ? FaultAction::kRetry : FaultAction::kDisable;
  }
  return FaultAction::kStop;
}

/** @brief Parses a policy name. */
[[nodiscard]] constexpr bool parse_fault_policy(std::string_view name, FaultPolicy* out) noexcept {
  if (name == "stop") {
    *out = FaultPolicy::kStop;
    return true;
  }
  if (name == "disable") {
    *out = FaultPolicy::kDisable;
    return true;
  }
  if (name == "retry") {
    *out = FaultPolicy::kRetry;
    return true;
  }
  return false;
}

/** @brief Stable name for diagnostics. */
[[nodiscard]] constexpr const char* to_string(FaultPolicy policy) noexcept {
  switch (policy) {
    case FaultPolicy::kStop:
      return "stop";
    case FaultPolicy::kDisable:
      return "disable";
    case FaultPolicy::kRetry:
      return "retry";
  }
  return "unknown";
}

}  // namespace srsp::core

#endif  // SRSP_CORE_FAULT_POLICY_HPP