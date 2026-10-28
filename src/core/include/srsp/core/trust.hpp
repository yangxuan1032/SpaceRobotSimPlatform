/**
 * @file trust.hpp
 * @brief Plugin trust levels and the in-process admission policy.
 *
 * docs/02 section 16 defines four levels. The rule that matters for the loader
 * is: a manifest declaring T2 (isolated) or T3 (development) must not be
 * loaded in-process — those belong to a sidecar or an explicit development
 * mode. Note that a manifest declaration is authorisation *input*, not proof
 * that the OS sandbox exists.
 */
#ifndef SRSP_CORE_TRUST_HPP
#define SRSP_CORE_TRUST_HPP

#include <cstdint>
#include <string_view>

namespace srsp::core {

/** @brief Trust level of a plugin, ordered from most to least trusted. */
enum class TrustLevel : std::uint8_t {
  kBuiltin = 0,      ///< T0: shipped with the platform.
  kManaged = 1,      ///< T1: user-installed, signed, in-process allowed.
  kIsolated = 2,     ///< T2: must run in a sidecar with resource limits.
  kDevelopment = 3,  ///< T3: unsigned development plugin, never for release.
};

/** @brief True when the level may be loaded into this process. */
[[nodiscard]] constexpr bool allows_in_process(TrustLevel level) noexcept {
  return level == TrustLevel::kBuiltin || level == TrustLevel::kManaged;
}

/**
 * @brief Parses a manifest value.
 * @param value Integer from the manifest (0..3).
 * @param out Receives the level when the value is valid.
 * @return False for out-of-range values.
 */
[[nodiscard]] constexpr bool parse_trust_level(int value, TrustLevel* out) noexcept {
  if (out == nullptr || value < 0 || value > 3) {
    return false;
  }
  *out = static_cast<TrustLevel>(value);
  return true;
}

/** @brief Stable short name used in diagnostics. */
[[nodiscard]] constexpr std::string_view to_string(TrustLevel level) noexcept {
  switch (level) {
    case TrustLevel::kBuiltin:
      return "T0";
    case TrustLevel::kManaged:
      return "T1";
    case TrustLevel::kIsolated:
      return "T2";
    case TrustLevel::kDevelopment:
      return "T3";
  }
  return "unknown";
}

}  // namespace srsp::core

#endif  // SRSP_CORE_TRUST_HPP
