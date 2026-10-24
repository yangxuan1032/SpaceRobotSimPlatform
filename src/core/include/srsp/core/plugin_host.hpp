/**
 * @file plugin_host.hpp
 * @brief Loading, lifetime and invocation of in-process SRSP plugins.
 *
 * The host owns one dynamically loaded plugin library at a time. It validates
 * the ABI major version, the returned vtable and its `struct_size` before any
 * call is forwarded, and guarantees `shutdown` runs before the library is
 * unloaded.
 */
#ifndef SRSP_CORE_PLUGIN_HOST_HPP
#define SRSP_CORE_PLUGIN_HOST_HPP

#include <srsp/plugin_api.h>

#include <string>
#include <string_view>

namespace srsp::core {

/**
 * @brief Owns a loaded plugin library and forwards lifecycle calls to it.
 *
 * Lifetime rules:
 * - `load()` must succeed before `init()` / `call()` / `checkpoint()` /
 *   `restore()` are used; otherwise SRSP_E_NOT_READY is returned.
 * - `unload()` is idempotent and is also called from the destructor.
 * - The host services table is copied on construction; the caller keeps
 *   ownership of the pointed-to state.
 */
class PluginHost {
 public:
  /**
   * @brief Creates a host with the given kernel service table.
   * @param services Host services handed to the plugin on `query`/`init`.
   */
  explicit PluginHost(srsp_host_v1 services) noexcept;
  ~PluginHost();

  PluginHost(const PluginHost&) = delete;
  PluginHost& operator=(const PluginHost&) = delete;
  PluginHost(PluginHost&&) = delete;
  PluginHost& operator=(PluginHost&&) = delete;

  /**
   * @brief Opens the library and resolves the plugin entry point.
   * @param library_path Absolute or relative path to the shared library.
   * @param err Optional error details.
   * @return SRSP_OK on success; SRSP_E_NOT_FOUND if the file cannot be opened;
   *         SRSP_E_VERSION_MISMATCH if the plugin rejects the host ABI;
   *         SRSP_E_INVALID_ARGUMENT if the vtable is incomplete;
   *         SRSP_E_INVALID_STATE if a plugin is already loaded.
   */
  srsp_status load(std::string_view library_path, srsp_error* err) noexcept;

  /**
   * @brief Loads the plugin described by an SRSP manifest (`plugin.json`).
   *
   * The manifest is the source of truth for the plugin id and the ABI major
   * version; its `transport.library` entry selects the library for the current
   * platform. Validation happens before the library is opened, so a mismatch
   * never loads foreign code.
   *
   * @param manifest_path Path to `plugin.json`.
   * @param err Optional error details.
   * @return SRSP_OK, SRSP_E_NOT_FOUND (manifest or library missing),
   *         SRSP_E_INVALID_ARGUMENT (required fields missing),
   *         SRSP_E_VERSION_MISMATCH (manifest or plugin rejects the ABI).
   */
  srsp_status load_with_manifest(std::string_view manifest_path,
                                 srsp_error* err) noexcept;

  /** @brief Calls `shutdown` (if initialised) and unloads the library. */
  void unload() noexcept;

  /** @brief True when a library is loaded and its vtable passed validation. */
  [[nodiscard]] bool loaded() const noexcept { return plugin_ != nullptr; }

  /** @brief True when `init()` succeeded and `shutdown` has not run yet. */
  [[nodiscard]] bool initialized() const noexcept { return initialized_; }

  /** @brief Returns the validated vtable, or nullptr when not loaded. */
  [[nodiscard]] const srsp_plugin_v1* plugin() const noexcept { return plugin_; }

  /** @brief Plugin id from the manifest; empty when loaded by raw path. */
  [[nodiscard]] const std::string& plugin_id() const noexcept {
    return plugin_id_;
  }

  /**
   * @brief Initialises the plugin with a JSON configuration.
   * @param config_json UTF-8 JSON, not NUL terminated.
   * @param err Optional error details.
   */
  srsp_status init(srsp_str config_json, srsp_error* err) noexcept;

  /**
   * @brief Invokes a capability method on the plugin.
   * @param method Stable method name, e.g. "core.ping".
   * @param request Encoded request bytes (may be empty).
   * @param response Receives a buffer allocated by the plugin.
   * @param err Optional error details.
   */
  srsp_status call(srsp_str method, srsp_bytes request, srsp_buffer* response,
                   srsp_error* err) noexcept;

  /** @brief Serialises plugin state; SRSP_E_UNSUPPORTED if not implemented. */
  srsp_status checkpoint(srsp_buffer* out, srsp_error* err) noexcept;

  /** @brief Restores plugin state; SRSP_E_UNSUPPORTED if not implemented. */
  srsp_status restore(srsp_bytes in, srsp_error* err) noexcept;

 private:
  srsp_host_v1 host_{};
  void* library_ = nullptr;
  const srsp_plugin_v1* plugin_ = nullptr;
  std::string plugin_id_;
  bool initialized_ = false;
};

}  // namespace srsp::core

#endif  // SRSP_CORE_PLUGIN_HOST_HPP
