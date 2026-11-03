/**
 * @file test_event.cpp
 * @brief Unit tests for deterministic event keys and CommandQueue (D2).
 */
#include <srsp/core/event.hpp>

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using srsp::core::Command;
using srsp::core::CommandQueue;
using srsp::core::EventKey;
using srsp::core::PushResult;

namespace {

Command make(std::int64_t t_ns, std::int32_t priority, std::uint32_t source, std::uint64_t seq,
             std::uint32_t kind = 0) {
  Command command;
  command.key = EventKey{t_ns, priority, source, seq};
  command.kind = kind;
  return command;
}

}  // namespace

TEST_CASE("keys order by time, priority, source then sequence", "[event]") {
  const EventKey a{100, 0, 2, 1};
  const EventKey b{100, 0, 1, 9};  // lower source wins even with a higher seq
  const EventKey c{100, 1, 0, 0};  // lower priority wins over source
  const EventKey d{99, 5, 5, 5};   // earlier time always wins

  CHECK(b < a);
  CHECK_FALSE(a < b);
  CHECK(a < c);
  CHECK(d < c);
}

TEST_CASE("commands pop in key order regardless of submission order", "[event]") {
  CommandQueue queue(8);
  REQUIRE(queue.push(make(200, 0, 0, 1, 3)) == PushResult::kAccepted);
  REQUIRE(queue.push(make(100, 0, 2, 1, 1)) == PushResult::kAccepted);
  REQUIRE(queue.push(make(100, 0, 1, 5, 2)) == PushResult::kAccepted);

  const auto due = queue.pop_until(200);
  REQUIRE(due.size() == 3);
  CHECK(due[0].kind == 2);  // t=100, source=1
  CHECK(due[1].kind == 1);  // t=100, source=2
  CHECK(due[2].kind == 3);  // t=200
}

TEST_CASE("time bound is inclusive and leaves later work queued", "[event]") {
  CommandQueue queue(8);
  REQUIRE(queue.push(make(100, 0, 0, 1)) == PushResult::kAccepted);
  REQUIRE(queue.push(make(101, 0, 0, 2)) == PushResult::kAccepted);

  const auto due = queue.pop_until(100);
  REQUIRE(due.size() == 1);
  CHECK(due[0].key.t_tai_ns == 100);
  CHECK(queue.size() == 1);
}

TEST_CASE("a full queue rejects instead of dropping", "[event]") {
  CommandQueue queue(2);
  REQUIRE(queue.push(make(1, 0, 0, 1)) == PushResult::kAccepted);
  REQUIRE(queue.push(make(2, 0, 0, 2)) == PushResult::kAccepted);
  CHECK(queue.push(make(3, 0, 0, 3)) == PushResult::kRejectedFull);
  CHECK(queue.rejected_full() == 1);
  CHECK(queue.size() == 2);
  CHECK(queue.accepted() == 2);
}

TEST_CASE("invalid submissions are counted separately", "[event]") {
  CommandQueue queue(4);
  CHECK(queue.push(make(-1, 0, 0, 1)) == PushResult::kRejectedInvalid);

  Command huge = make(10, 0, 0, 1);
  huge.payload.resize(CommandQueue::kMaxPayloadBytes + 1);
  CHECK(queue.push(std::move(huge)) == PushResult::kRejectedInvalid);
  CHECK(queue.rejected_invalid() == 2);
  CHECK(queue.size() == 0);
}