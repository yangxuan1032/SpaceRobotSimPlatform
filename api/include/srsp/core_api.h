/*
 * SRSP Host-side C API (draft)
 * ----------------------------
 * Stable, language-neutral boundary for APPLICATIONS embedding the SRSP
 * kernel: srsp-cli, the Python SDK, srsp-web and other tools. This is the
 * mirror image of plugin_api.h, which covers the "kernel calls plugin"
 * direction; this header covers the "application calls kernel" direction.
 *
 * Status: DRAFT 0.1 (see docs/02-plugin-contract.md and the multilang/
 * toolchain plan). Breaking changes are allowed until 1.0; after that the
 * same evolution rules as the plugin ABI apply (fields only appended,
 * struct_size versioning).
 *
 * Rules (mirroring plugin_api.h):
 *   - Pure C99. No C++ types, STL, exceptions or RTTI may cross this boundary.
 *   - Every config struct starts with `struct_size`; fields are only appended.
 *   - Strings and byte buffers are UTF-8 / binary, length-delimited, NOT
 *     NUL-terminated.
 *   - Buffers returned by the kernel (srsp_get_snapshot, srsp_plugin_call)
 *     are released with srsp_buffer_release; no other free may be used.
 *   - All functions may be called from any thread unless documented
 *     otherwise; callbacks fire on the simulation thread.
 *   - Snapshot payloads are Arrow IPC bytes (see docs/03-data-contracts.md);
 *     checkpoint/restore and get_snapshot share one serialization path.
 */

#ifndef SRSP_CORE_API_H
#define SRSP_CORE_API_H

#include <srsp/plugin_api.h> /* srsp_str/bytes/buffer, srsp_status, srsp_error, srsp_log_fn */

#define SRSP_CORE_API_MAJOR 0u
#define SRSP_CORE_API_MINOR 1u

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque kernel context. Owned by the caller between create and destroy. */
typedef struct srsp_context srsp_context;

/* Event/topic callback. Fires on the simulation thread; the payload view is
 * valid only for the duration of the call. */
typedef void (SRSP_CALL* srsp_event_fn)(void* user, srsp_str topic,
                                        srsp_bytes payload, uint64_t schema_hash);

enum {
  SRSP_CTX_FLAG_NONE = 0u,
  SRSP_CTX_FLAG_ALLOW_UNSIGNED = 1u << 0, /* load unsigned plugins (dev) */
  SRSP_CTX_FLAG_HEADLESS = 1u << 1        /* no renderer/UI services */
};

typedef struct srsp_context_config {
  uint32_t struct_size;
  srsp_log_fn log;   /* may be NULL: no log forwarding */
  void* log_user;
  srsp_str data_dir; /* may be {NULL,0}: schema/data root override */
  uint32_t flags;    /* SRSP_CTX_FLAG_* bitmask */
} srsp_context_config;

typedef uint64_t srsp_subscription; /* 0 = invalid */

/* Version as (major << 16) | minor. */
SRSP_EXPORT uint32_t SRSP_CALL srsp_core_api_version(void);

/* Lifecycle. `config` may be NULL for defaults. */
SRSP_EXPORT srsp_status SRSP_CALL srsp_context_create(
    const srsp_context_config* config, srsp_context** out, srsp_error* err);
SRSP_EXPORT void SRSP_CALL srsp_context_destroy(srsp_context* ctx);

/* Project load/unload (see docs/03-data-contracts.md section 8). */
SRSP_EXPORT srsp_status SRSP_CALL srsp_load_project(srsp_context* ctx,
                                                    srsp_str path,
                                                    srsp_error* err);
SRSP_EXPORT srsp_status SRSP_CALL srsp_unload_project(srsp_context* ctx,
                                                      srsp_error* err);

/* Simulation clock control. */
SRSP_EXPORT srsp_status SRSP_CALL srsp_start(srsp_context* ctx, srsp_error* err);
SRSP_EXPORT srsp_status SRSP_CALL srsp_pause(srsp_context* ctx, srsp_error* err);
SRSP_EXPORT srsp_status SRSP_CALL srsp_step(srsp_context* ctx, uint64_t steps,
                                            srsp_error* err);
SRSP_EXPORT int64_t SRSP_CALL srsp_time_tai_ns(const srsp_context* ctx);

/* Immutable snapshot of the current state as Arrow IPC bytes. Release with
 * srsp_buffer_release. */
SRSP_EXPORT srsp_status SRSP_CALL srsp_get_snapshot(srsp_context* ctx,
                                                    srsp_buffer* out,
                                                    srsp_error* err);
SRSP_EXPORT void SRSP_CALL srsp_buffer_release(srsp_context* ctx,
                                               srsp_buffer* buffer);

/* Topic subscription (docs/03 section 5). Wildcards follow the topic rules. */
SRSP_EXPORT srsp_status SRSP_CALL srsp_subscribe(srsp_context* ctx,
                                                 srsp_str topic,
                                                 srsp_event_fn callback,
                                                 void* user,
                                                 srsp_subscription* out,
                                                 srsp_error* err);
SRSP_EXPORT srsp_status SRSP_CALL srsp_unsubscribe(srsp_context* ctx,
                                                   srsp_subscription id,
                                                   srsp_error* err);

/* Generic capability passthrough: same method+payload semantics as the plugin
 * ABI `call` and the sidecar `Invoke` RPC. */
SRSP_EXPORT srsp_status SRSP_CALL srsp_plugin_call(srsp_context* ctx,
                                                   srsp_str plugin_id,
                                                   srsp_str method,
                                                   srsp_bytes request,
                                                   srsp_buffer* response,
                                                   srsp_error* err);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SRSP_CORE_API_H */
