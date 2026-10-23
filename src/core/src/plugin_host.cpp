/**
 * @file plugin_host.cpp
 * @brief Platform-neutral implementation of srsp::core::PluginHost.
 */
#include <srsp/core/plugin_host.hpp>

#include <cstdint>
#include <utility>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <unistd.h>
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
  return attrs != INVALID_FILE_ATTRIBUTES &&
         (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
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

PluginHost::~PluginHost() { unload(); }

srsp_status PluginHost::load(std::string_view library_path,
                             srsp_error* err) noexcept {
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

  auto query = reinterpret_cast<srsp_plugin_query_fn>(
      find_symbol(handle, SRSP_PLUGIN_ENTRY));
  if (query == nullptr) {
    close_library(handle);
    set_error(err, SRSP_E_NOT_FOUND, "plugin entry point is missing");
    return SRSP_E_NOT_FOUND;
  }

  const srsp_plugin_v1* plugin = query(SRSP_ABI_MAJOR, &host_);
  if (plugin == nullptr) {
    close_library(handle);
    set_error(err, SRSP_E_VERSION_MISMATCH,
              "plugin does not support the host ABI major version");
    return SRSP_E_VERSION_MISMATCH;
  }

  if (plugin->struct_size < sizeof(srsp_plugin_v1) || plugin->init == nullptr ||
      plugin->call == nullptr) {
    close_library(handle);
    set_error(err, SRSP_E_INVALID_ARGUMENT,
              "plugin vtable is truncated or incomplete");
    return SRSP_E_INVALID_ARGUMENT;
  }

  library_ = handle;
  plugin_ = plugin;
  initialized_ = false;
  return SRSP_OK;
}

void PluginHost::unload() noexcept {
  if (plugin_ != nullptr && initialized_ && plugin_->shutdown != nullptr) {
    plugin_->shutdown(plugin_->self);
  }
  initialized_ = false;
  plugin_ = nullptr;
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

  const srsp_status status =
      plugin_->init(plugin_->self, &host_, config_json, err);
  if (status == SRSP_OK) {
    initialized_ = true;
  }
  return status;
}

srsp_status PluginHost::call(srsp_str method, srsp_bytes request,
                             srsp_buffer* response, srsp_error* err) noexcept {
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

}  // namespace srsp::core
