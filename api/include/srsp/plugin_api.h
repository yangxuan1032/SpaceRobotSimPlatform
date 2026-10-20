/*
 * SRSP Plugin C ABI
 * -----------------
 * Stable, language-neutral boundary between the SRSP kernel and in-process
 * plugins.
 *
 * Rules (see docs/02-plugin-contract.md):
 *   - Pure C99. No C++ types, STL, exceptions or RTTI may cross this boundary.
 *   - Every struct starts with `struct_size`; fields are only appended.
 *   - Strings and byte buffers are UTF-8 / binary, length-delimited, and are
 *     NOT NUL-terminated.
 *   - Buffers returned through `srsp_buffer` are allocated with
 *     `host->alloc` and must be released with `host->free` by the caller.
 *   - Plugin methods are invoked on the simulation thread unless the manifest
 *     lists them under `entry.thread_safe`.
 *
 * ABI version: 1.0
 */

#ifndef SRSP_PLUGIN_API_H
#define SRSP_PLUGIN_API_H

#include <stddef.h>
#include <stdint.h>

#define SRSP_ABI_MAJOR 1u
#define SRSP_ABI_MINOR 0u

/* Exported entry point looked up by the kernel. */
#define SRSP_PLUGIN_ENTRY "srsp_plugin_query"

#if defined(_WIN32)
#  define SRSP_EXPORT __declspec(dllexport)
#  define SRSP_CALL __cdecl
#else
#  define SRSP_EXPORT __attribute__((visibility("default")))
#  define SRSP_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Basic value types                                                         */
/* ------------------------------------------------------------------------- */

typedef struct srsp_str {
  const char* data; /* UTF-8, not NUL-terminated */
  uint64_t size;
} srsp_str;

typedef struct srsp_bytes {
  const uint8_t* data; /* read-only view owned by the caller */
  uint64_t size;
} srsp_bytes;

typedef struct srsp_buffer {
  uint8_t* data; /* allocated by the callee via host->alloc */
  uint64_t size;
} srsp_buffer;

/* Log levels, compatible with common logging conventions. */
enum {
  SRSP_LOG_TRACE = 0,
  SRSP_LOG_DEBUG = 1,
  SRSP_LOG_INFO = 2,
  SRSP_LOG_WARN = 3,
  SRSP_LOG_ERROR = 4,
  SRSP_LOG_CRITICAL = 5
};

typedef enum srsp_status {
  SRSP_OK = 0,
  SRSP_E_INVALID_ARGUMENT = 1,
  SRSP_E_UNSUPPORTED = 2,
  SRSP_E_NOT_READY = 3,
  SRSP_E_INVALID_STATE = 4,
  SRSP_E_NOT_FOUND = 5,
  SRSP_E_TIMEOUT = 6,
  SRSP_E_OUT_OF_MEMORY = 7,
  SRSP_E_BUFFER_TOO_SMALL = 8,
  SRSP_E_VERSION_MISMATCH = 9,
  SRSP_E_CANCELLED = 10,
  SRSP_E_INTERNAL = 11
} srsp_status;

typedef struct srsp_error {
  uint32_t struct_size;
  srsp_status code;
  const char* message;     /* short, user-facing; owned by callee */
  const char* detail_json; /* optional structured context; owned by callee */
} srsp_error;

/* ------------------------------------------------------------------------- */
/* Services provided by the kernel to plugins                                */
/* ------------------------------------------------------------------------- */

typedef void (SRSP_CALL* srsp_log_fn)(void* user, int32_t level, srsp_str message);
typedef double (SRSP_CALL* srsp_time_fn)(void* user);
typedef srsp_status (SRSP_CALL* srsp_alloc_fn)(void* user, uint64_t size,
                                               uint64_t alignment, void** out);
typedef void (SRSP_CALL* srsp_free_fn)(void* user, void* ptr);
typedef srsp_status (SRSP_CALL* srsp_publish_fn)(void* user, srsp_str topic,
                                                 srsp_bytes payload,
                                                 uint64_t schema_hash);
typedef srsp_status (SRSP_CALL* srsp_shm_map_fn)(void* user, srsp_str name,
                                                 uint64_t size, int32_t writable,
                                                 void** out);
typedef void (SRSP_CALL* srsp_shm_unmap_fn)(void* user, void* ptr, uint64_t size);

typedef struct srsp_host_v1 {
  uint32_t struct_size;
  void* user;

  srsp_log_fn log;
  srsp_time_fn time_tai;

  srsp_alloc_fn alloc;
  srsp_free_fn free;

  srsp_publish_fn publish;

  srsp_shm_map_fn shm_map;
  srsp_shm_unmap_fn shm_unmap;
} srsp_host_v1;

/* ------------------------------------------------------------------------- */
/* Plugin vtable                                                             */
/* ------------------------------------------------------------------------- */

typedef srsp_status (SRSP_CALL* srsp_init_fn)(void* self, const srsp_host_v1* host,
                                              srsp_str config_json, srsp_error* err);
typedef srsp_status (SRSP_CALL* srsp_call_fn)(void* self, srsp_str method,
                                              srsp_bytes request,
                                              srsp_buffer* response, srsp_error* err);
typedef srsp_status (SRSP_CALL* srsp_checkpoint_fn)(void* self, srsp_buffer* out,
                                                    srsp_error* err);
typedef srsp_status (SRSP_CALL* srsp_restore_fn)(void* self, srsp_bytes in,
                                                 srsp_error* err);
typedef void (SRSP_CALL* srsp_shutdown_fn)(void* self);

typedef struct srsp_plugin_v1 {
  uint32_t struct_size;
  const char* id;      /* must match plugin.json */
  const char* version; /* semver */
  void* self;

  srsp_init_fn init;
  srsp_call_fn call;
  srsp_checkpoint_fn checkpoint; /* optional, may be NULL */
  srsp_restore_fn restore;       /* optional, may be NULL */
  srsp_shutdown_fn shutdown;     /* optional, may be NULL */
} srsp_plugin_v1;

/*
 * Entry point. The kernel passes the ABI major version it speaks and the host
 * services table. Return NULL when the plugin cannot serve that ABI major.
 * The returned table must remain valid for the lifetime of the process.
 */
typedef const srsp_plugin_v1* (SRSP_CALL* srsp_plugin_query_fn)(
    uint32_t host_abi_major, const srsp_host_v1* host);

SRSP_EXPORT const srsp_plugin_v1* SRSP_CALL srsp_plugin_query(
    uint32_t host_abi_major, const srsp_host_v1* host);

/* ------------------------------------------------------------------------- */
/* Helpers (C/C++ inline, no runtime dependency)                             */
/* ------------------------------------------------------------------------- */

#define SRSP_STR_LITERAL(lit) \
  { (lit), (uint64_t)(sizeof(lit) - 1u) }

#define SRSP_BYTES_NONE { NULL, 0u }

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SRSP_PLUGIN_API_H */
