/**
 * @file registry.hpp
 * @brief Plugin and capability registry owned by the kernel.
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
struct PluginRecord {
  std::string id;            ///< Reverse-DNS plugin id, unique.
  std::string version;       ///< SemVer string as reported by the plugin.
  std::string library_path;  ///< Resolved path of the loaded library.
  bool loaded = false;       ///< True after the host loaded the library.
};

/** @brief Record of one capability implementation. */
struct CapabilityRecord {
  std::string name;       ///< Capability name without the major, e.g. srsp.dynamics/solver.
  uint32_t major = 1;     ///< Capability contract major version.
  std::string plugin_id;  ///< Owning plugin id.
  int priority = 0;       ///< Higher wins when several implementations match.
};

/**
 * @brief Registry of plugins and their capabilities.
 *
 * Not thread-safe: the kernel drives it from the simulation thread only.
 */
class Registry {
 public:
  /** @brief Registers a plugin; false when the id already exists. */
  bool register_plugin(PluginRecord record);

  /** @brief Removes a plugin and every capability it owns; false if absent. */
  bool unregister_plugin(std::string_view id);

  /** @brief Marks a plugin as loaded; false if the id is unknown. */
  bool mark_loaded(std::string_view id, bool loaded);

  /** @brief Registers a capability implementation; false if the plugin is unknown. */
  bool add_capability(CapabilityRecord record);

  /** @brief Returns the plugin record, or nullptr when absent. */
  [[nodiscard]] const PluginRecord* find_plugin(std::string_view id) const;

  /**
   * @brief Resolves the best implementation of a capability.
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
