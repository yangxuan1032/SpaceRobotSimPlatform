// ABI layout contract test.
//
// These assertions must hold simultaneously on MSVC, GCC and Clang (x86-64,
// Windows LLP64 and Linux LP64 alike). If any of them fails, the C ABI is no
// longer portable across toolchains and an in-process plugin built with one
// toolchain cannot be hosted by another (see docs/02-plugin-contract.md
// section 4.1 and the rules block in plugin_api.h).

#include <srsp/plugin_api.h>
#include <srsp/core_api.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

#define SRSP_CHECK_SIZE(type, bytes) \
  static_assert(sizeof(type) == (bytes), #type " size changed; ABI break")
#define SRSP_CHECK_ALIGN(type, bytes) \
  static_assert(alignof(type) == (bytes), #type " alignment changed; ABI break")
#define SRSP_CHECK_OFF(type, field, bytes) \
  static_assert(offsetof(type, field) == (bytes), #type "." #field " offset changed; ABI break")

/* ---- scalar ground rules -------------------------------------------------- */

static_assert(sizeof(void*) == 8, "the ABI assumes 64-bit pointers");
static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");
static_assert(sizeof(int64_t) == 8, "int64_t must be 8 bytes");
static_assert(sizeof(double) == 8, "double must be IEEE-754 binary64");
static_assert(sizeof(srsp_status) == 4, "enums must keep a fixed 32-bit representation");

/* ---- basic value types ----------------------------------------------------- */

SRSP_CHECK_OFF(srsp_str, data, 0);
SRSP_CHECK_OFF(srsp_str, size, 8);
SRSP_CHECK_SIZE(srsp_str, 16);
SRSP_CHECK_ALIGN(srsp_str, 8);

SRSP_CHECK_OFF(srsp_bytes, data, 0);
SRSP_CHECK_OFF(srsp_bytes, size, 8);
SRSP_CHECK_SIZE(srsp_bytes, 16);
SRSP_CHECK_ALIGN(srsp_bytes, 8);

SRSP_CHECK_OFF(srsp_buffer, data, 0);
SRSP_CHECK_OFF(srsp_buffer, size, 8);
SRSP_CHECK_SIZE(srsp_buffer, 16);
SRSP_CHECK_ALIGN(srsp_buffer, 8);

SRSP_CHECK_OFF(srsp_error, struct_size, 0);
SRSP_CHECK_OFF(srsp_error, code, 4);
SRSP_CHECK_OFF(srsp_error, message, 8);
SRSP_CHECK_OFF(srsp_error, detail_json, 16);
SRSP_CHECK_SIZE(srsp_error, 24);
SRSP_CHECK_ALIGN(srsp_error, 8);

/* ---- host services table --------------------------------------------------- */

SRSP_CHECK_OFF(srsp_host_v1, struct_size, 0);
SRSP_CHECK_OFF(srsp_host_v1, user, 8);
SRSP_CHECK_OFF(srsp_host_v1, log, 16);
SRSP_CHECK_OFF(srsp_host_v1, time_tai, 24);
SRSP_CHECK_OFF(srsp_host_v1, alloc, 32);
SRSP_CHECK_OFF(srsp_host_v1, free, 40);
SRSP_CHECK_OFF(srsp_host_v1, publish, 48);
SRSP_CHECK_OFF(srsp_host_v1, shm_map, 56);
SRSP_CHECK_OFF(srsp_host_v1, shm_unmap, 64);
SRSP_CHECK_OFF(srsp_host_v1, time_tai_ns, 72);
SRSP_CHECK_SIZE(srsp_host_v1, 80);
SRSP_CHECK_ALIGN(srsp_host_v1, 8);

/* ---- plugin vtable --------------------------------------------------------- */

SRSP_CHECK_OFF(srsp_plugin_v1, struct_size, 0);
SRSP_CHECK_OFF(srsp_plugin_v1, id, 8);
SRSP_CHECK_OFF(srsp_plugin_v1, version, 16);
SRSP_CHECK_OFF(srsp_plugin_v1, self, 24);
SRSP_CHECK_OFF(srsp_plugin_v1, init, 32);
SRSP_CHECK_OFF(srsp_plugin_v1, call, 40);
SRSP_CHECK_OFF(srsp_plugin_v1, checkpoint, 48);
SRSP_CHECK_OFF(srsp_plugin_v1, restore, 56);
SRSP_CHECK_OFF(srsp_plugin_v1, shutdown, 64);
SRSP_CHECK_SIZE(srsp_plugin_v1, 72);
SRSP_CHECK_ALIGN(srsp_plugin_v1, 8);

/* ---- host-side core API (draft 0.1) ---------------------------------------- */

SRSP_CHECK_OFF(srsp_context_config, struct_size, 0);
SRSP_CHECK_OFF(srsp_context_config, log, 8);
SRSP_CHECK_OFF(srsp_context_config, log_user, 16);
SRSP_CHECK_OFF(srsp_context_config, data_dir, 24);
SRSP_CHECK_OFF(srsp_context_config, flags, 40);
SRSP_CHECK_SIZE(srsp_context_config, 48);
SRSP_CHECK_ALIGN(srsp_context_config, 8);

SRSP_CHECK_SIZE(srsp_subscription, 8);

/* ---- version constants ------------------------------------------------------ */

static_assert(SRSP_ABI_MAJOR == 1u, "ABI major changed; bump deliberately");
static_assert(SRSP_ABI_MINOR == 0u, "ABI minor changed; bump deliberately");

int main() {
  std::printf("srsp_str        sizeof=%zu alignof=%zu\n", sizeof(srsp_str), alignof(srsp_str));
  std::printf("srsp_bytes      sizeof=%zu alignof=%zu\n", sizeof(srsp_bytes), alignof(srsp_bytes));
  std::printf("srsp_buffer     sizeof=%zu alignof=%zu\n", sizeof(srsp_buffer), alignof(srsp_buffer));
  std::printf("srsp_error      sizeof=%zu alignof=%zu\n", sizeof(srsp_error), alignof(srsp_error));
  std::printf("srsp_host_v1    sizeof=%zu alignof=%zu\n", sizeof(srsp_host_v1), alignof(srsp_host_v1));
  std::printf("srsp_plugin_v1  sizeof=%zu alignof=%zu\n", sizeof(srsp_plugin_v1), alignof(srsp_plugin_v1));
  std::printf("srsp_context_config sizeof=%zu alignof=%zu\n", sizeof(srsp_context_config), alignof(srsp_context_config));
  std::printf("abi layout: all static asserts passed\n");
  return 0;
}
