/**
 * @file plugin_host.cpp
 * @brief Platform-neutral implementation of srsp::core::PluginHost.
 */
#include <srsp/core/plugin_host.hpp>

#if defined(SRSP_HAVE_MANIFEST)
#include <srsp/core/trust.hpp>

#include <nlohmann/json.hpp>
#endif

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

#include <cstdlib>
#include <exception>
#include <fstream>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace srsp::core {
namespace {

void set_error(srsp_error* err, srsp_status code, const char* message) noexcept {
  if (err == nullptr) {
    return;
  }
  err->struct_size = sizeof(srsp_error);
  err->code = code;
  err->message = message;
  err->detail_json = nullptr;
}

#if defined(_WIN32)
using LibraryHandle = HMODULE;

LibraryHandle open_library(const std::string& path) noexcept {
  return ::LoadLibraryA(path.c_str());
}

void* find_symbol(LibraryHandle handle, const char* name) noexcept {
  return reinterpret_cast<void*>(::GetProcAddress(handle, name));
}

void close_library(LibraryHandle handle) noexcept {
  if (handle != nullptr) {
    ::FreeLibrary(handle);
  }
}

bool library_exists(const std::string& path) noexcept {
  const DWORD attrs = ::GetFileAttributesA(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
#else
using LibraryHandle = void*;

LibraryHandle open_library(const std::string& path) noexcept {
  return ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
}

void* find_symbol(LibraryHandle handle, const char* name) noexcept {
  return ::dlsym(handle, name);
}

void close_library(LibraryHandle handle) noexcept {
  if (handle != nullptr) {
    ::dlclose(handle);
  }
}

bool library_exists(const std::string& path) noexcept {
  return ::access(path.c_str(), R_OK) == 0;
}
#endif

}  // namespace

PluginHost::PluginHost(srsp_host_v1 services) noexcept : host_(services) {}

PluginHost::~PluginHost() {
  unload();
}

srsp_status PluginHost::load(std::string_view library_path, srsp_error* err) noexcept {
  if (plugin_ != nullptr) {
    set_error(err, SRSP_E_INVALID_STATE, "a plugin is already loaded");
    return SRSP_E_INVALID_STATE;
  }
  if (library_path.empty()) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "library path is empty");
    return SRSP_E_INVALID_ARGUMENT;
  }

  const std::string path(library_path);
  if (!library_exists(path)) {
    set_error(err, SRSP_E_NOT_FOUND, "plugin library does not exist");
    return SRSP_E_NOT_FOUND;
  }

  LibraryHandle handle = open_library(path);
  if (handle == nullptr) {
    set_error(err, SRSP_E_NOT_FOUND, "plugin library could not be opened");
    return SRSP_E_NOT_FOUND;
  }

  auto query = reinterpret_cast<srsp_plugin_query_fn>(find_symbol(handle, SRSP_PLUGIN_ENTRY));
  if (query == nullptr) {
    close_library(handle);
    set_error(err, SRSP_E_NOT_FOUND, "plugin entry point is missing");
    return SRSP_E_NOT_FOUND;
  }

  const srsp_plugin_v1* plugin = query(SRSP_ABI_MAJOR, &host_);
  if (plugin == nullptr) {
    close_library(handle);
    set_error(err, SRSP_E_VERSION_MISMATCH, "plugin does not support the host ABI major version");
    return SRSP_E_VERSION_MISMATCH;
  }

  if (plugin->struct_size < sizeof(srsp_plugin_v1) || plugin->init == nullptr ||
      plugin->call == nullptr) {
    close_library(handle);
    set_error(err, SRSP_E_INVALID_ARGUMENT, "plugin vtable is truncated or incomplete");
    return SRSP_E_INVALID_ARGUMENT;
  }

  library_ = handle;
  plugin_ = plugin;
  plugin_id_.clear();
  initialized_ = false;
  return SRSP_OK;
}

void PluginHost::unload() noexcept {
  if (plugin_ != nullptr && initialized_ && plugin_->shutdown != nullptr) {
    plugin_->shutdown(plugin_->self);
  }
  initialized_ = false;
  plugin_ = nullptr;
  plugin_id_.clear();
  if (library_ != nullptr) {
    close_library(static_cast<LibraryHandle>(library_));
    library_ = nullptr;
  }
}

srsp_status PluginHost::init(srsp_str config_json, srsp_error* err) noexcept {
  if (plugin_ == nullptr) {
    set_error(err, SRSP_E_NOT_READY, "no plugin is loaded");
    return SRSP_E_NOT_READY;
  }
  if (initialized_) {
    set_error(err, SRSP_E_INVALID_STATE, "plugin is already initialised");
    return SRSP_E_INVALID_STATE;
  }

  const srsp_status status = plugin_->init(plugin_->self, &host_, config_json, err);
  if (status == SRSP_OK) {
    initialized_ = true;
  }
  return status;
}

srsp_status PluginHost::call(srsp_str method, srsp_bytes request, srsp_buffer* response,
                             srsp_error* err) noexcept {
  if (plugin_ == nullptr) {
    set_error(err, SRSP_E_NOT_READY, "no plugin is loaded");
    return SRSP_E_NOT_READY;
  }
  if (!initialized_) {
    set_error(err, SRSP_E_NOT_READY, "plugin is not initialised");
    return SRSP_E_NOT_READY;
  }
  return plugin_->call(plugin_->self, method, request, response, err);
}

srsp_status PluginHost::checkpoint(srsp_buffer* out, srsp_error* err) noexcept {
  if (plugin_ == nullptr || !initialized_) {
    set_error(err, SRSP_E_NOT_READY, "plugin is not initialised");
    return SRSP_E_NOT_READY;
  }
  if (plugin_->checkpoint == nullptr) {
    set_error(err, SRSP_E_UNSUPPORTED, "plugin does not implement checkpoint");
    return SRSP_E_UNSUPPORTED;
  }
  return plugin_->checkpoint(plugin_->self, out, err);
}

srsp_status PluginHost::restore(srsp_bytes in, srsp_error* err) noexcept {
  if (plugin_ == nullptr || !initialized_) {
    set_error(err, SRSP_E_NOT_READY, "plugin is not initialised");
    return SRSP_E_NOT_READY;
  }
  if (plugin_->restore == nullptr) {
    set_error(err, SRSP_E_UNSUPPORTED, "plugin does not implement restore");
    return SRSP_E_UNSUPPORTED;
  }
  return plugin_->restore(plugin_->self, in, err);
}

srsp_status PluginHost::load_with_manifest(std::string_view manifest_path,
                                           srsp_error* err) noexcept {
#if !defined(SRSP_HAVE_MANIFEST)
  (void)manifest_path;
  set_error(err, SRSP_E_UNSUPPORTED, "manifest parsing is unavailable in this build");
  return SRSP_E_UNSUPPORTED;
#else
  if (plugin_ != nullptr) {
    set_error(err, SRSP_E_INVALID_STATE, "a plugin is already loaded");
    return SRSP_E_INVALID_STATE;
  }
  if (manifest_path.empty()) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "manifest path is empty");
    return SRSP_E_INVALID_ARGUMENT;
  }

  const std::filesystem::path manifest_file{std::string(manifest_path)};
  std::ifstream input(manifest_file, std::ios::binary);
  if (!input) {
    set_error(err, SRSP_E_NOT_FOUND, "plugin manifest does not exist");
    return SRSP_E_NOT_FOUND;
  }

  nlohmann::json document;
  try {
    input >> document;
  } catch (const std::exception&) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "plugin manifest is not valid JSON");
    return SRSP_E_INVALID_ARGUMENT;
  }

  if (!document.is_object() || !document.contains("id") || !document.contains("api") ||
      !document.contains("transport")) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "plugin manifest lacks id, api or transport");
    return SRSP_E_INVALID_ARGUMENT;
  }

  const std::string id = document["id"].get<std::string>();
  const std::string api = document["api"].get<std::string>();
  if (id.empty() || api.empty()) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "plugin manifest has empty id or api");
    return SRSP_E_INVALID_ARGUMENT;
  }

  const unsigned long manifest_major = std::strtoul(api.c_str(), nullptr, 10);
  if (manifest_major != SRSP_ABI_MAJOR) {
    set_error(err, SRSP_E_VERSION_MISMATCH, "manifest api major does not match the host ABI");
    return SRSP_E_VERSION_MISMATCH;
  }

  const auto& transport = document["transport"];
  if (!transport.is_object() || !transport.contains("library") ||
      !transport["library"].is_object()) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "plugin manifest lacks transport.library");
    return SRSP_E_INVALID_ARGUMENT;
  }

  const auto& libraries = transport["library"];
#if defined(_WIN32)
  const char* platform_key = "windows-x86_64";
#else
  const char* platform_key = "linux-x86_64";
#endif
  if (!libraries.contains(platform_key)) {
    set_error(err, SRSP_E_NOT_FOUND, "manifest has no library entry for this platform");
    return SRSP_E_NOT_FOUND;
  }

  std::filesystem::path library{std::string(libraries[platform_key].get<std::string>())};
  if (library.is_relative()) {
    library = manifest_file.parent_path() / library;
  }

  const srsp_status status = load(library.string(), err);
  if (status == SRSP_OK) {
    plugin_id_ = id;
  }
  return status;
#endif
}

}  // namespace srsp::core
