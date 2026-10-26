// Minimal host that loads an SRSP plugin through the C ABI.
// Build: cmake --build --preset <preset> --target srsp_hello_host
// Run:   srsp_hello_host <path-to-plugin-library>

#include <srsp/plugin_api.h>

#include <cstring>
#include <string>

#include <cstdio>

#if defined(_WIN32)
#include <malloc.h>
#include <windows.h>
#else
#include <cstdlib>
#include <dlfcn.h>
#endif

namespace {

struct DemoHost {
  double sim_time = 0.0;
  int log_count = 0;
};

/* ---- host services ------------------------------------------------------ */

void SRSP_CALL host_log(void* user, int32_t level, srsp_str message) {
  auto* host = static_cast<DemoHost*>(user);
  static const char* kLevels[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRIT"};
  const char* name = (level >= 0 && level <= 5) ? kLevels[level] : "LOG";
  std::printf("[%s] %.*s\n", name, static_cast<int>(message.size), message.data);
  ++host->log_count;
}

double SRSP_CALL host_time_tai(void* user) {
  return static_cast<DemoHost*>(user)->sim_time;
}

int64_t SRSP_CALL host_time_tai_ns(void* user) {
  return static_cast<int64_t>(static_cast<DemoHost*>(user)->sim_time * 1e9);
}

srsp_status SRSP_CALL host_alloc(void* /*user*/, uint64_t size, uint64_t alignment, void** out) {
  if (out == nullptr) {
    return SRSP_E_INVALID_ARGUMENT;
  }
  if (alignment < sizeof(void*)) {
    alignment = sizeof(void*);
  }
#if defined(_WIN32)
  *out = _aligned_malloc(static_cast<size_t>(size), static_cast<size_t>(alignment));
  return *out != nullptr ? SRSP_OK : SRSP_E_OUT_OF_MEMORY;
#else
  return posix_memalign(out, static_cast<size_t>(alignment), static_cast<size_t>(size)) == 0
             ? SRSP_OK
             : SRSP_E_OUT_OF_MEMORY;
#endif
}

void SRSP_CALL host_free(void* /*user*/, void* ptr) {
#if defined(_WIN32)
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

srsp_status SRSP_CALL host_publish(void* user, srsp_str topic, srsp_bytes payload,
                                   uint64_t /*schema_hash*/) {
  auto* host = static_cast<DemoHost*>(user);
  std::printf("[PUBLISH t=%.3f] %.*s (%llu bytes)\n", host->sim_time, static_cast<int>(topic.size),
              topic.data, static_cast<unsigned long long>(payload.size));
  return SRSP_OK;
}

srsp_status SRSP_CALL host_shm_map(void* /*user*/, srsp_str /*name*/, uint64_t /*size*/,
                                   int32_t /*writable*/, void** /*out*/) {
  return SRSP_E_UNSUPPORTED;
}

void SRSP_CALL host_shm_unmap(void* /*user*/, void* /*ptr*/, uint64_t /*size*/) {}

/* ---- dynamic library ---------------------------------------------------- */

#if defined(_WIN32)
using LibraryHandle = HMODULE;

LibraryHandle open_library(const char* path) {
  return LoadLibraryA(path);
}

void* find_symbol(LibraryHandle handle, const char* name) {
  return reinterpret_cast<void*>(GetProcAddress(handle, name));
}

void close_library(LibraryHandle handle) {
  if (handle != nullptr) {
    FreeLibrary(handle);
  }
}

std::string last_library_error() {
  const DWORD code = GetLastError();
  char* buffer = nullptr;
  FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, code, 0, reinterpret_cast<char*>(&buffer), 0, nullptr);
  std::string message = buffer != nullptr ? buffer : "unknown error";
  LocalFree(buffer);
  return message;
}
#else
using LibraryHandle = void*;

LibraryHandle open_library(const char* path) {
  return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void* find_symbol(LibraryHandle handle, const char* name) {
  return dlsym(handle, name);
}

void close_library(LibraryHandle handle) {
  if (handle != nullptr) {
    dlclose(handle);
  }
}

std::string last_library_error() {
  const char* error = dlerror();
  return error != nullptr ? error : "unknown error";
}
#endif

/* ---- demo --------------------------------------------------------------- */

bool invoke(const srsp_plugin_v1* plugin, DemoHost& host, const char* method, std::string* out) {
  srsp_buffer response{nullptr, 0};
  srsp_error error{sizeof(srsp_error), SRSP_OK, nullptr, nullptr};
  const srsp_str name{method, static_cast<uint64_t>(std::strlen(method))};
  const srsp_bytes request{nullptr, 0};

  const srsp_status status = plugin->call(plugin->self, name, request, &response, &error);
  if (status != SRSP_OK) {
    std::printf("  ! %s failed: code=%d %s\n", method, static_cast<int>(status),
                error.message != nullptr ? error.message : "");
    return false;
  }
  if (out != nullptr && response.data != nullptr) {
    out->assign(reinterpret_cast<const char*>(response.data), static_cast<size_t>(response.size));
  }
  host_free(&host, response.data);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "srsp_hello.dll";

  DemoHost host;
  srsp_host_v1 host_api{sizeof(srsp_host_v1), &host,           host_log,     host_time_tai,
                        host_alloc,           host_free,       host_publish, host_shm_map,
                        host_shm_unmap,       host_time_tai_ns};

  LibraryHandle library = open_library(path);
  if (library == nullptr) {
    std::fprintf(stderr, "cannot open plugin '%s': %s\n", path, last_library_error().c_str());
    return 1;
  }

  auto query = reinterpret_cast<srsp_plugin_query_fn>(find_symbol(library, SRSP_PLUGIN_ENTRY));
  if (query == nullptr) {
    std::fprintf(stderr, "missing entry point '%s'\n", SRSP_PLUGIN_ENTRY);
    close_library(library);
    return 2;
  }

  const srsp_plugin_v1* plugin = query(SRSP_ABI_MAJOR, &host_api);
  if (plugin == nullptr) {
    std::fprintf(stderr, "plugin rejected ABI %u\n", SRSP_ABI_MAJOR);
    close_library(library);
    return 3;
  }

  std::printf("loaded %s %s (ABI %u.%u)\n", plugin->id, plugin->version, SRSP_ABI_MAJOR,
              SRSP_ABI_MINOR);

  srsp_error error{sizeof(srsp_error), SRSP_OK, nullptr, nullptr};
  const srsp_status init_status =
      plugin->init != nullptr
          ? plugin->init(plugin->self, &host_api, SRSP_STR_LITERAL("{}"), &error)
          : SRSP_E_INVALID_STATE;
  if (init_status != SRSP_OK) {
    std::fprintf(stderr, "init failed: %s\n", error.message != nullptr ? error.message : "unknown");
    close_library(library);
    return 4;
  }

  std::string text;
  std::printf("\n== core.describe ==\n");
  invoke(plugin, host, "core.describe", &text);
  std::printf("%s\n", text.c_str());

  for (int i = 0; i < 3; ++i) {
    host.sim_time += 0.001;
    invoke(plugin, host, "controller.update", nullptr);
  }

  srsp_buffer checkpoint{nullptr, 0};
  if (plugin->checkpoint != nullptr) {
    plugin->checkpoint(plugin->self, &checkpoint, &error);
  }
  std::printf("\ncheckpoint: %llu bytes at step 3\n",
              static_cast<unsigned long long>(checkpoint.size));

  for (int i = 0; i < 2; ++i) {
    invoke(plugin, host, "controller.update", nullptr);
  }
  invoke(plugin, host, "core.ping", &text);
  std::printf("after 2 more steps: %s\n", text.c_str());

  if (plugin->restore != nullptr) {
    const srsp_bytes payload{checkpoint.data, checkpoint.size};
    if (plugin->restore(plugin->self, payload, &error) != SRSP_OK) {
      std::fprintf(stderr, "restore failed: %s\n",
                   error.message != nullptr ? error.message : "unknown");
    }
  }
  host_free(&host, checkpoint.data);

  invoke(plugin, host, "core.ping", &text);
  std::printf("after restore:      %s\n", text.c_str());

  invoke(plugin, host, "core.ping", &text);
  invoke(plugin, host, "controller.update", &text);
  std::printf("after one update:   %s\n", text.c_str());

  if (plugin->shutdown != nullptr) {
    plugin->shutdown(plugin->self);
  }
  close_library(library);
  std::printf("\nlog lines: %d\n", host.log_count);
  return 0;
}
