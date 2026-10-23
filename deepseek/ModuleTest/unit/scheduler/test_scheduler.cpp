/**
 * @file test_scheduler.cpp
 * @brief Unit tests for srsp::core::Scheduler.
 */
#include <srsp/core/diagnostics.hpp>
#include <srsp/core/scheduler.hpp>
#include <srsp/core/time_service.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

using srsp::core::Diagnostics;
using srsp::core::Scheduler;
using srsp::core::Severity;
using srsp::core::TimeService;

namespace {
constexpr int64_t kOneMs = 1'000'000;
}

TEST_CASE("scheduler runs the requested number of steps", "[scheduler]") {
  TimeService clock(kOneMs, 0);
  Scheduler scheduler(clock);

  std::vector<int64_t> times;
  const uint64_t executed = scheduler.run(
      5, [&times](uint64_t, int64_t t_ns) { times.push_back(t_ns); });

  CHECK(executed == 5);
  CHECK(scheduler.total_steps() == 5);
  REQUIRE(times.size() == 5);
  CHECK(times.front() == kOneMs);
  CHECK(times.back() == 5 * kOneMs);
}

TEST_CASE("scheduler passes the step index after advancing", "[scheduler]") {
  TimeService clock(kOneMs, 0);
  Scheduler scheduler(clock);

  std::vector<uint64_t> indices;
  scheduler.run(3, [&indices](uint64_t index, int64_t) { indices.push_back(index); });

  REQUIRE(indices.size() == 3);
  CHECK(indices[0] == 1u);
  CHECK(indices[2] == 3u);
}

TEST_CASE("scheduler stops when the clock is paused", "[scheduler]") {
  TimeService clock(kOneMs, 0);
  Diagnostics diagnostics;
  Scheduler scheduler(clock, &diagnostics);

  clock.pause();
  const uint64_t executed = scheduler.run(10, [](uint64_t, int64_t) {});

  CHECK(executed == 0);
  CHECK(scheduler.total_steps() == 0);
  CHECK(diagnostics.count(Severity::kDebug) == 1);
}

TEST_CASE("scheduler step returns false while paused", "[scheduler]") {
  TimeService clock(kOneMs, 0);
  Scheduler scheduler(clock);

  REQUIRE(scheduler.step(nullptr));
  clock.pause();
  CHECK_FALSE(scheduler.step(nullptr));
  CHECK(scheduler.total_steps() == 1);
}
