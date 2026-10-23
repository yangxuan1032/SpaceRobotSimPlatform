/**
 * @file test_time_service.cpp
 * @brief Unit tests for srsp::core::TimeService.
 */
#include <srsp/core/time_service.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using srsp::core::TimeService;

namespace {
constexpr int64_t kOneMs = 1'000'000;  ///< One millisecond in nanoseconds.
}

TEST_CASE("time service accumulates macro steps without drift", "[time]") {
  TimeService clock(kOneMs, 0);

  for (int i = 0; i < 1000; ++i) {
    REQUIRE(clock.advance());
  }

  CHECK(clock.step_index() == 1000u);
  CHECK(clock.now_ns() == 1000 * kOneMs);
}

TEST_CASE("time service honours pause and resume", "[time]") {
  TimeService clock(kOneMs, 0);
  REQUIRE(clock.advance());

  clock.pause();
  CHECK_FALSE(clock.running());
  CHECK_FALSE(clock.advance());
  CHECK(clock.now_ns() == kOneMs);

  clock.resume();
  REQUIRE(clock.advance());
  CHECK(clock.now_ns() == 2 * kOneMs);
}

TEST_CASE("time service rejects a non-positive step", "[time]") {
  TimeService clock(kOneMs, 0);
  CHECK_FALSE(clock.set_step_ns(0));
  CHECK_FALSE(clock.set_step_ns(-5));
  CHECK(clock.step_ns() == kOneMs);

  CHECK(clock.set_step_ns(2 * kOneMs));
  REQUIRE(clock.advance());
  CHECK(clock.now_ns() == 2 * kOneMs);
}

TEST_CASE("time service resets time and step index", "[time]") {
  TimeService clock(kOneMs, 0);
  REQUIRE(clock.advance(5));
  REQUIRE(clock.now_ns() == 5 * kOneMs);

  clock.reset(42);
  CHECK(clock.now_ns() == 42);
  CHECK(clock.step_index() == 0u);
  CHECK(clock.running());
}

TEST_CASE("time service advances several steps at once", "[time]") {
  TimeService clock(kOneMs, 100);
  REQUIRE(clock.advance(3));
  CHECK(clock.now_ns() == 100 + 3 * kOneMs);
  CHECK(clock.step_index() == 3u);
}
