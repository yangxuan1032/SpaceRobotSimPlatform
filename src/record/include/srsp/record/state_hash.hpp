/**
 * @file state_hash.hpp
 * @brief Canonical state hashing (XXH3-128) used by replay verification.
 *
 * The hash is computed over the canonical little-endian encoding of the state
 * vector: doubles are copied as IEEE-754 bits in the order the dynamics layer
 * produces them (base position, quaternion, joint positions). Two runs that
 * produce the same state produce the same 128-bit digest; replay compares the
 * digests step by step instead of comparing floating-point values.
 */
#ifndef SRSP_RECORD_STATE_HASH_HPP
#define SRSP_RECORD_STATE_HASH_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace srsp::record {

/** @brief 128-bit digest. */
struct StateHash {
  std::uint64_t high = 0;
  std::uint64_t low = 0;

  [[nodiscard]] bool operator==(const StateHash&) const noexcept = default;

  /** @brief Lower-case hex form, 32 characters. */
  [[nodiscard]] std::string to_hex() const;
};

/** @brief Hashes raw bytes with XXH3-128. */
StateHash hash_bytes(const void* data, std::size_t size);

/** @brief Hashes a double vector through its canonical bit pattern. */
StateHash hash_state(const std::vector<double>& values);

}  // namespace srsp::record

#endif  // SRSP_RECORD_STATE_HASH_HPP
