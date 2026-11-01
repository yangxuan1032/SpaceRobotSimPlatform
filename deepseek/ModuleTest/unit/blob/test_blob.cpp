/**
 * @file test_blob.cpp
 * @brief Unit tests for BlobStore generation and lease semantics (D4).
 */
#include <srsp/core/blob.hpp>

#include <catch2/catch_test_macros.hpp>

using srsp::core::BlobBytes;
using srsp::core::BlobRef;
using srsp::core::BlobStore;

namespace {
BlobBytes payload(std::uint8_t value, std::size_t size = 16) {
  return BlobBytes(size, value);
}
}  // namespace

TEST_CASE("stored blobs are readable and accounted", "[blob]") {
  BlobStore store;
  const BlobRef ref = store.store(payload(0xAB, 32));
  CHECK(ref.valid());
  CHECK(ref.size_bytes == 32u);
  CHECK(store.size() == 1u);
  CHECK(store.stored_bytes() == 32u);

  const auto lease = store.acquire(ref);
  REQUIRE(lease.has_value());
  CHECK((*lease)->size() == 32u);
  CHECK((*lease)->front() == 0xAB);
}

TEST_CASE("a stale generation is rejected", "[blob]") {
  BlobStore store;
  const BlobRef first = store.store(payload(0x11));
  store.release(first);

  const BlobRef second = store.store(payload(0x22));
  BlobRef stale = second;
  stale.generation = first.generation;
  CHECK_FALSE(store.acquire(stale).has_value());
  CHECK(store.acquire(second).has_value());
}

TEST_CASE("leases outlive the store entry", "[blob]") {
  BlobStore store;
  const BlobRef ref = store.store(payload(0x33, 8));
  auto lease = store.acquire(ref);
  REQUIRE(lease.has_value());

  store.release(ref);
  CHECK(store.size() == 0u);
  CHECK(store.stored_bytes() == 0u);
  CHECK((*lease)->size() == 8u);  // shared ownership keeps the bytes alive
  CHECK_FALSE(store.acquire(ref).has_value());

  lease.reset();
  CHECK(store.size() == 0u);
}

TEST_CASE("releasing an unknown ref is a no-op", "[blob]") {
  BlobStore store;
  const BlobRef unknown{};
  store.release(unknown);
  CHECK(store.size() == 0u);
}