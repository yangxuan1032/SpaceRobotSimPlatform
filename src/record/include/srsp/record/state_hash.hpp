/**
 * @file state_hash.hpp
 * @brief Canonical state hashing (XXH3-128) used by replay verification.
 * @brief_zh 回放校验使用的规范化状态哈希（XXH3-128）。
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
/** @brief_zh 128 位摘要。 */
struct StateHash {
  std::uint64_t high = 0;
  std::uint64_t low = 0;

  [[nodiscard]] bool operator==(const StateHash&) const noexcept = default;

  /** @brief Lower-case hex form, 32 characters. */
  /** @brief_zh 小写十六进制形式，32 个字符。 */
  [[nodiscard]] std::string to_hex() const;
};

/** @brief Hashes raw bytes with XXH3-128. */
/** @brief_zh 用 XXH3-128 对原始字节求哈希。 */
StateHash hash_bytes(const void* data, std::size_t size);

/** @brief Hashes a double vector through its canonical bit pattern. */
/** @brief_zh 按规范位模式对 double 向量求哈希。 */
StateHash hash_state(const std::vector<double>& values);

}  // namespace srsp::record

#endif  // SRSP_RECORD_STATE_HASH_HPP
