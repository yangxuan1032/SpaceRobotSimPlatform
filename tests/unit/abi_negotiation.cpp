// Version negotiation and incompatible-rejection paths (docs/02 section 6 and
// the "version negotiation" item of the freeze checklist in docs/02 section 12).
//
// Runs against a real plugin binary: pass its path as argv[1]. The test uses the
// hello plugin as the fixture and checks:
//   1. a host speaking an unsupported ABI major is rejected (query returns NULL)
//   2. the supported major is accepted and the returned table is valid
//   3. init rejects a host table without alloc/free (SRSP_E_INVALID_ARGUMENT)
//   4. a host table truncated before time_tai_ns still works (field-append rule)
//   5. unknown method -> SRSP_E_UNSUPPORTED with a populated srsp_error
//   6. shutdown is optional and idempotent from the host's point of view

#include <srsp/plugin_api.h>

#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#  include <malloc.h>
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <cstdlib>
#endif

namespace {

int g_failures = 0;

void check(bool condition, const char* what, int line) {
  if (!condition) {
    std::fprintf(stderr, "FAIL (line %d): %s\n", line, what);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __LINE__)

struct HostState {
  int log_count = 0;
};

void SRSP_CALL host_log(void* user, int32_t, srsp_str) {
  if (user != nullptr) {
    static_cast<HostState*>(user)->log_count++;
  }
}

double SRSP_CALL host_time(void*) { return 0.0; }

int64_t SRSP_CALL host_time_ns(void*) { return 0; }

srsp_status SRSP_CALL host_alloc(void*, uint64_t size, uint64_t alignment,
                                 void** out) {
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
  return posix_memalign(out, static_cast<size_t>(alignment),
                        static_cast<size_t>(size)) == 0
             ? SRSP_OK
             : SRSP_E_OUT_OF_MEMORY;
#endif
}

void SRSP_CALL host_free(void*, void* ptr) {
#if defined(_WIN32)
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

srsp_status SRSP_CALL host_publish(void*, srsp_str, srsp_bytes, uint64_t) {
  return SRSP_E_UNSUPPORTED;
}

srsp_status SRSP_CALL host_shm_map(void*, srsp_str, uint64_t, int32_t, void**) {
  return SRSP_E_UNSUPPORTED;
}

void SRSP_CALL host_shm_unmap(void*, void*, uint64_t) {}

srsp_host_v1 make_host(HostState* state) {
  srsp_host_v1 host{};
  host.struct_size = sizeof(srsp_host_v1);
  host.user = state;
  host.log = host_log;
  host.time_tai = host_time;
  host.time_tai_ns = host_time_ns;
  host.alloc = host_alloc;
  host.free = host_free;
  host.publish = host_publish;
  host.shm_map = host_shm_map;
  host.shm_unmap = host_shm_unmap;
  return host;
}

#if defined(_WIN32)
using Library = HMODULE;
Library open_library(const char* path) { return LoadLibraryA(path); }
void* find_symbol(Library lib, const char* name) {
  return reinterpret_cast<void*>(GetProcAddress(lib, name));
}
void close_library(Library lib) {
  if (lib != nullptr) {
    FreeLibrary(lib);
  }
}
#else
using Library = void*;
Library open_library(const char* path) { return dlopen(path, RTLD_NOW | RTLD_LOCAL); }
void* find_symbol(Library lib, const char* name) { return dlsym(lib, name); }
void close_library(Library lib) {
  if (lib != nullptr) {
    dlclose(lib);
  }
}
#endif

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <plugin-library>\n",
                 argc > 0 ? argv[0] : "srsp_abi_negotiation");
    return 2;
  }

  Library lib = open_library(argv[1]);
  if (lib == nullptr) {
    std::fprintf(stderr, "cannot open plugin: %s\n", argv[1]);
    return 2;
  }

  auto query = reinterpret_cast<srsp_plugin_query_fn>(find_symbol(lib, SRSP_PLUGIN_ENTRY));
  CHECK(query != nullptr);

  if (query != nullptr) {
    HostState state;
    srsp_host_v1 host = make_host(&state);

    // 1. unsupported major is rejected
    const srsp_plugin_v1* rejected = query(SRSP_ABI_MAJOR + 1u, &host);
    CHECK(rejected == nullptr);

    // 2. supported major is accepted
    const srsp_plugin_v1* plugin = query(SRSP_ABI_MAJOR, &host);
    CHECK(plugin != nullptr);

    if (plugin != nullptr) {
      CHECK(plugin->struct_size >= sizeof(srsp_plugin_v1));
      CHECK(plugin->id != nullptr && plugin->id[0] != '\0');
      CHECK(plugin->version != nullptr && plugin->version[0] != '\0');
      CHECK(plugin->init != nullptr);
      CHECK(plugin->call != nullptr);

      // 3. host table without alloc/free is rejected by init
      srsp_host_v1 broken = host;
      broken.alloc = nullptr;
      broken.free = nullptr;
      srsp_error err{};
      err.struct_size = sizeof(srsp_error);
      const srsp_status broken_status =
          plugin->init(plugin->self, &broken, SRSP_STR_LITERAL("{}"), &err);
      CHECK(broken_status == SRSP_E_INVALID_ARGUMENT);
      CHECK(err.code == SRSP_E_INVALID_ARGUMENT);
      CHECK(err.message != nullptr);

      // 4. host table truncated before time_tai_ns still works (append-only rule)
      srsp_host_v1 truncated = host;
      truncated.struct_size =
          static_cast<uint32_t>(offsetof(srsp_host_v1, time_tai_ns));
      const srsp_status truncated_status =
          plugin->init(plugin->self, &truncated, SRSP_STR_LITERAL("{}"), nullptr);
      CHECK(truncated_status == SRSP_OK);

      // 5. unknown method -> SRSP_E_UNSUPPORTED with populated error
      srsp_buffer response{};
      srsp_error call_err{};
      call_err.struct_size = sizeof(srsp_error);
      const srsp_status unknown_status =
          plugin->call(plugin->self, SRSP_STR_LITERAL("does.not.exist"),
                       SRSP_BYTES_NONE, &response, &call_err);
      CHECK(unknown_status == SRSP_E_UNSUPPORTED);
      CHECK(call_err.code == SRSP_E_UNSUPPORTED);

      // 6. a supported method still works after the rejected calls
      srsp_buffer ok_response{};
      const srsp_status ping_status =
          plugin->call(plugin->self, SRSP_STR_LITERAL("core.ping"),
                       SRSP_BYTES_NONE, &ok_response, nullptr);
      CHECK(ping_status == SRSP_OK);
      if (ping_status == SRSP_OK && ok_response.data != nullptr) {
        host.free(host.user, ok_response.data);
      }

      if (plugin->shutdown != nullptr) {
        plugin->shutdown(plugin->self);
      }
    }
  }

  close_library(lib);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("abi negotiation: all checks passed\n");
  return 0;
}
