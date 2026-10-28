/**
 * @file test_snapshot.cpp
 * @brief Unit tests for snapshot generation/lease semantics (D1).
 */
#include <srsp/core/snapshot.hpp>

#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using srsp::core::SnapshotId;
using srsp::core::SnapshotStore;

namespace {

SnapshotStore::Bytes make_bytes(std::uint8_t value, std::size_t size = 8) {
  return SnapshotStore::Bytes(size, value);
}

}  // namespace

TEST_CASE("published snapshots carry step and generation", "[snapshot]") {
  SnapshotStore store;
  const auto a = store.publish(10, make_bytes(1).data(), 8);
  const auto b = store.publish(11, make_bytes(2).data(), 8);

  CHECK(a.step_index == 10);
  CHECK(b.step_index == 11);
  CHECK(b.generation == a.generation + 1);
  CHECK(store.latest_id() == b);
}

TEST_CASE("a lease survives eviction of the store entry", "[snapshot]") {
  SnapshotStore store(2);  // keeps only two entries
  const auto a = store.publish(1, make_bytes(0xAA).data(), 8);
  auto lease = store.acquire(a);
  REQUIRE(lease.has_value());
  REQUIRE(lease->valid());

  store.publish(2, make_bytes(0xBB).data(), 8);
  store.publish(3, make_bytes(0xCC).data(), 8);  // evicts `a`
  CHECK_FALSE(store.acquire(a).has_value());
  // The outstanding lease still reads the old, immutable bytes.
  CHECK(lease->bytes() == make_bytes(0xAA));
}

TEST_CASE("stale ids are never silently served new data", "[snapshot]") {
  SnapshotStore store(1);
  const auto a = store.publish(1, make_bytes(0x11).data(), 8);
  const auto b = store.publish(2, make_bytes(0x22).data(), 8);

  const auto stale = store.acquire(a);
  REQUIRE_FALSE(stale.has_value());
  const auto fresh = store.acquire(b);
  REQUIRE(fresh.has_value());
  CHECK(fresh->bytes() == make_bytes(0x22));
}

TEST_CASE("release drops the lease reference", "[snapshot]") {
  SnapshotStore store;
  const auto id = store.publish(7, make_bytes(0x33).data(), 4);
  auto lease = store.acquire(id);
  REQUIRE(lease.has_value());
  lease->release();
  CHECK_FALSE(lease->valid());
}