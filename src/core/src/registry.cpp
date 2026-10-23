/**
 * @file registry.cpp
 * @brief Implementation of the plugin and capability registry.
 */
#include <srsp/core/registry.hpp>

#include <algorithm>

namespace srsp::core {

bool Registry::register_plugin(PluginRecord record) {
  if (record.id.empty() || find_plugin(record.id) != nullptr) {
    return false;
  }
  plugins_.push_back(std::move(record));
  return true;
}

bool Registry::unregister_plugin(std::string_view id) {
  const auto it = std::find_if(plugins_.begin(), plugins_.end(),
                               [id](const PluginRecord& p) { return p.id == id; });
  if (it == plugins_.end()) {
    return false;
  }
  plugins_.erase(it);
  capabilities_.erase(
      std::remove_if(capabilities_.begin(), capabilities_.end(),
                     [id](const CapabilityRecord& c) { return c.plugin_id == id; }),
      capabilities_.end());
  return true;
}

bool Registry::mark_loaded(std::string_view id, bool loaded) {
  for (auto& plugin : plugins_) {
    if (plugin.id == id) {
      plugin.loaded = loaded;
      return true;
    }
  }
  return false;
}

bool Registry::add_capability(CapabilityRecord record) {
  if (record.name.empty() || record.plugin_id.empty() ||
      find_plugin(record.plugin_id) == nullptr) {
    return false;
  }
  capabilities_.push_back(std::move(record));
  return true;
}

const PluginRecord* Registry::find_plugin(std::string_view id) const {
  const auto it = std::find_if(plugins_.begin(), plugins_.end(),
                               [id](const PluginRecord& p) { return p.id == id; });
  return it == plugins_.end() ? nullptr : &*it;
}

const CapabilityRecord* Registry::resolve(std::string_view name,
                                          uint32_t major) const {
  const CapabilityRecord* best = nullptr;
  for (const auto& capability : capabilities_) {
    if (capability.name != name || capability.major != major) {
      continue;
    }
    // Strictly greater keeps the earliest registered on ties.
    if (best == nullptr || capability.priority > best->priority) {
      best = &capability;
    }
  }
  return best;
}

}  // namespace srsp::core
