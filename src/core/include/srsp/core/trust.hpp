/**
 * @file trust.hpp
 * @brief Plugin trust levels and the in-process admission policy.
 * @brief_zh 插件信任级别与进程内准入策略。
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
/** @brief_zh 插件信任级别，从最可信到最不可信。 */
enum class TrustLevel : std::uint8_t {
  kBuiltin = 0,  ///< T0: shipped with the platform. / 随平台发布。
  kManaged =
      1,  ///< T1: user-installed, signed, in-process allowed. / 用户安装且已签名，允许进程内加载。
  kIsolated =
      2,  ///< T2: must run in a sidecar with resource limits. / 必须在带资源限制的边车中运行。
  kDevelopment = 3,  ///< T3: unsigned development plugin, never for release. /
                     ///< 未签名的开发插件，禁止用于发布。
};

/** @brief True when the level may be loaded into this process. */
/** @brief_zh 该级别是否允许加载进当前进程。 */
[[nodiscard]] constexpr bool allows_in_process(TrustLevel level) noexcept {
  return level == TrustLevel::kBuiltin || level == TrustLevel::kManaged;
}

/**
 * @brief Parses a manifest value.
 * @brief_zh 解析清单中的取值。
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
/** @brief_zh 诊断中使用的稳定短名。 */
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
