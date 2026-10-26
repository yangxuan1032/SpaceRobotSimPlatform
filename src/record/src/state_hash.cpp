/**
 * @file state_hash.cpp
 * @brief XXH3-128 based state hashing.
 */
#include <srsp/record/state_hash.hpp>

#include <cstdio>
#include <xxhash.h>

namespace srsp::record {

std::string StateHash::to_hex() const {
  char buffer[33];
  std::snprintf(buffer, sizeof(buffer), "%016llx%016llx", static_cast<unsigned long long>(high),
                static_cast<unsigned long long>(low));
  return buffer;
}

StateHash hash_bytes(const void* data, std::size_t size) {
  const XXH128_hash_t digest = XXH3_128bits(data, size);
  StateHash result;
  result.high = digest.high64;
  result.low = digest.low64;
  return result;
}

StateHash hash_state(const std::vector<double>& values) {
  static_assert(sizeof(double) == 8, "state hashing assumes IEEE-754 doubles");
  return hash_bytes(values.data(), values.size() * sizeof(double));
}

}  // namespace srsp::record
