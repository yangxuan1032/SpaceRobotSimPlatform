/**
 * @file test_fault_policy.cpp
 * @brief Fault policy decision table (B4).
 */
#include <srsp/core/fault_policy.hpp>

#include <catch2/catch_test_macros.hpp>

using srsp::core::decide_fault;
using srsp::core::FaultAction;
using srsp::core::FaultPolicy;
using srsp::core::parse_fault_policy;
using srsp::core::to_string;

TEST_CASE("stop and disable act on the first failure", "[fault_policy]") {
  CHECK(decide_fault(FaultPolicy::kStop, 1, 3) == FaultAction::kStop);
  CHECK(decide_fault(FaultPolicy::kDisable, 1, 3) == FaultAction::kDisable);
}

TEST_CASE("retry is bounded by the retry budget", "[fault_policy]") {
  CHECK(decide_fault(FaultPolicy::kRetry, 1, 3) == FaultAction::kRetry);
  CHECK(decide_fault(FaultPolicy::kRetry, 3, 3) == FaultAction::kRetry);
  CHECK(decide_fault(FaultPolicy::kRetry, 4, 3) == FaultAction::kDisable);
}

TEST_CASE("policy names parse and round-trip", "[fault_policy]") {
  FaultPolicy policy = FaultPolicy::kStop;
  REQUIRE(parse_fault_policy("retry", &policy));
  CHECK(policy == FaultPolicy::kRetry);
  CHECK(std::string_view(to_string(policy)) == "retry");
  CHECK_FALSE(parse_fault_policy("ignore", &policy));
}