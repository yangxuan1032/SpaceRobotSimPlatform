/**
 * @file registry.hpp
 * @brief Plugin and capability registry owned by the kernel.
 * @brief_zh 由内核持有的插件与能力注册表。
 *
 * The registry itself performs no loading; it only records what exists and
 * answers capability queries. Determinism rules: iteration follows insertion
 * order, capability resolution breaks priority ties by insertion order.
 */
#ifndef SRSP_CORE_REGISTRY_HPP
#define SRSP_CORE_REGISTRY_HPP

#include <srsp/plugin_api.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace srsp::core {

/** @brief Record of a discovered plugin. */
/** @brief_zh 已发现插件的记录。 */
struct PluginRecord {
  std::string id;            ///< Reverse-DNS plugin id, unique. / 反向域名唯一 id。
  std::string version;       ///< SemVer string as reported by the plugin. / 插件上报的 SemVer。
  std::string library_path;  ///< Resolved path of the loaded library. / 已加载库的解析路径。
  bool loaded = false;       ///< True after the host loaded the library. / 宿主加载后为真。
};

/** @brief Record of one capability implementation. */
/** @brief_zh 一个能力实现的记录。 */
struct CapabilityRecord {
  std::string name;       ///< Capability name without the major, e.g. srsp.dynamics/solver. /
                          ///< 不含主版本的能力名。
  uint32_t major = 1;     ///< Capability contract major version. / 能力契约主版本。
  std::string plugin_id;  ///< Owning plugin id. / 所属插件 id。
  int priority =
      0;  ///< Higher wins when several implementations match. / 多个实现匹配时数值大者胜出。
};

/**
 * @brief Registry of plugins and their capabilities.
 * @brief_zh 插件及其能力的注册表。
 *
 * Not thread-safe: the kernel drives it from the simulation thread only.
 */
class Registry {
 public:
  /** @brief Registers a plugin; false when the id already exists. */
  /** @brief_zh 注册插件；id 已存在时返回 false。 */
  bool register_plugin(PluginRecord record);

  /** @brief Removes a plugin and every capability it owns; false if absent. */
  /** @brief_zh 移除插件及其全部能力；不存在时返回 false。 */
  bool unregister_plugin(std::string_view id);

  /** @brief Marks a plugin as loaded; false if the id is unknown. */
  /** @brief_zh 标记插件已加载；id 未知时返回 false。 */
  bool mark_loaded(std::string_view id, bool loaded);

  /** @brief Registers a capability implementation; false if the plugin is unknown. */
  /** @brief_zh 注册能力实现；插件未知时返回 false。 */
  bool add_capability(CapabilityRecord record);

  /** @brief Returns the plugin record, or nullptr when absent. */
  /** @brief_zh 返回插件记录；不存在时为 nullptr。 */
  [[nodiscard]] const PluginRecord* find_plugin(std::string_view id) const;

  /**
   * @brief Resolves the best implementation of a capability.
   * @brief_zh 解析某能力的最佳实现。
   * @param name Capability name, e.g. "srsp.dynamics/solver".
   * @param major Contract major version.
   * @return The highest-priority record, or nullptr when none matches.
   */
  [[nodiscard]] const CapabilityRecord* resolve(std::string_view name, uint32_t major) const;

  [[nodiscard]] const std::vector<PluginRecord>& plugins() const noexcept { return plugins_; }
  [[nodiscard]] const std::vector<CapabilityRecord>& capabilities() const noexcept {
    return capabilities_;
  }

 private:
  std::vector<PluginRecord> plugins_;
  std::vector<CapabilityRecord> capabilities_;
};

}  // namespace srsp::core

#endif  // SRSP_CORE_REGISTRY_HPP
