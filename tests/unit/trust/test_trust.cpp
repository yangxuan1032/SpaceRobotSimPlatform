/**
 * @file test_trust.cpp
 * @brief Unit tests for the plugin trust-level admission policy (D3).
 */
#include <srsp/core/trust.hpp>

#include <catch2/catch_test_macros.hpp>

using srsp::core::allows_in_process;
using srsp::core::parse_trust_level;
using srsp::core::to_string;
using srsp::core::TrustLevel;

TEST_CASE("only builtin and managed plugins may load in-process", "[trust]") {
  CHECK(allows_in_process(TrustLevel::kBuiltin));
  CHECK(allows_in_process(TrustLevel::kManaged));
  CHECK_FALSE(allows_in_process(TrustLevel::kIsolated));
  CHECK_FALSE(allows_in_process(TrustLevel::kDevelopment));
}

TEST_CASE("trust levels parse only inside the documented range", "[trust]") {
  TrustLevel level = TrustLevel::kBuiltin;
  REQUIRE(parse_trust_level(2, &level));
  CHECK(level == TrustLevel::kIsolated);
  CHECK_FALSE(parse_trust_level(4, &level));
  CHECK_FALSE(parse_trust_level(-1, &level));
  CHECK_FALSE(parse_trust_level(1, nullptr));
}

TEST_CASE("trust levels have stable diagnostic names", "[trust]") {
  CHECK(to_string(TrustLevel::kBuiltin) == "T0");
  CHECK(to_string(TrustLevel::kManaged) == "T1");
  CHECK(to_string(TrustLevel::kIsolated) == "T2");
  CHECK(to_string(TrustLevel::kDevelopment) == "T3");
}