/**
 * @file core_api.h
 * @brief SRSP host-side C API: applications embedding the kernel.
 * @brief_zh SRSP 宿主侧 C API：嵌入内核的应用程序接口。
 *
 * Stable, language-neutral boundary for APPLICATIONS embedding the SRSP
 * kernel: srsp-cli, the Python SDK, srsp-web and other tools. This is the
 * mirror image of plugin_api.h, which covers the "kernel calls plugin"
 * direction; this header covers the "application calls kernel" direction.
 *
 * @zh 面向「应用程序调用内核」的稳定边界（srsp-cli、Python SDK、srsp-web 等）。
 * plugin_api.h 覆盖「内核调用插件」的反方向。
 *
 * Status: DRAFT 0.1 (see docs/02-plugin-contract.md). Breaking changes are
 * allowed until 1.0; afterwards the plugin ABI evolution rules apply (fields
 * only appended, struct_size versioning).
 *
 * @zh 状态：草案 0.1。1.0 之前允许破坏性变更；之后沿用插件 ABI 规则
 * （字段只追加、struct_size 版本化）。
 *
 * Rules (mirroring plugin_api.h):
 *   - Pure C99. No C++ types, STL, exceptions or RTTI may cross this boundary.
 *   - Every config struct starts with `struct_size`; fields are only appended.
 *   - Strings and byte buffers are UTF-8 / binary, length-delimited, NOT
 *     NUL-terminated.
 *   - Buffers returned by the kernel are released with srsp_buffer_release;
 *     no other free may be used.
 *   - All functions may be called from any thread unless documented
 *     otherwise; callbacks fire on the simulation thread.
 *   - Snapshot payloads are Arrow IPC bytes (docs/03-data-contracts.md);
 *     checkpoint/restore and get_snapshot share one serialization path.
 *
 * @zh 规则：纯 C99；配置结构体以 struct_size 开头且字段只追加；
 * 字符串/缓冲区为长度分隔的 UTF-8 或二进制（非 NUL 结尾）；
 * 内核返回的缓冲区必须用 srsp_buffer_release 释放；
 * 除非特别说明，函数可从任意线程调用，回调在仿真线程触发；
 * 快照载荷为 Arrow IPC 字节，checkpoint/restore 与 get_snapshot 共用一条序列化路径。
 */

#ifndef SRSP_CORE_API_H
#define SRSP_CORE_API_H

#include <srsp/plugin_api.h> /* srsp_str/bytes/buffer, srsp_status, srsp_error, srsp_log_fn */

#define SRSP_CORE_API_MAJOR 0u
#define SRSP_CORE_API_MINOR 1u

/**
 * @brief Library export/import decoration.
 * @brief_zh 库导出/导入修饰。
 *
 * The kernel defines SRSP_CORE_BUILD when compiling; applications consuming
 * the DLL must not define it.
 * @zh 内核编译时定义 SRSP_CORE_BUILD；使用 DLL 的应用不得定义它。
 */
#if defined(_WIN32)
#  if defined(SRSP_CORE_BUILD)
#    define SRSP_CORE_API __declspec(dllexport)
#  else
#    define SRSP_CORE_API __declspec(dllimport)
#  endif
#else
#  define SRSP_CORE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque kernel context, owned by the caller between create and destroy.
 * @brief_zh 不透明内核上下文，创建与销毁之间由调用方持有。
 */
typedef struct srsp_context srsp_context;

/**
 * @brief Event/topic callback.
 * @brief_zh 事件/主题回调。
 *
 * Fires on the simulation thread; the payload view is valid only for the
 * duration of the call.
 * @zh 在仿真线程触发；payload 视图仅在本次调用期间有效。
 */
typedef void (SRSP_CALL* srsp_event_fn)(void* user, srsp_str topic,
                                        srsp_bytes payload, uint64_t schema_hash);

/** @brief Context creation flags. @brief_zh 上下文创建标志。 */
enum {
  SRSP_CTX_FLAG_NONE = 0u,
  SRSP_CTX_FLAG_ALLOW_UNSIGNED = 1u << 0, /**< load unsigned plugins (dev) / 允许加载未签名插件（开发用） */
  SRSP_CTX_FLAG_HEADLESS = 1u << 1        /**< no renderer/UI services / 不启用渲染与 UI 服务 */
};

/**
 * @brief Context configuration; @c struct_size must be set first.
 * @brief_zh 上下文配置；必须先设置 struct_size。
 */
typedef struct srsp_context_config {
  uint32_t struct_size; /**< sizeof(srsp_context_config) / 结构体大小 */
  srsp_log_fn log;      /**< may be NULL: no log forwarding / 可为 NULL：不转发日志 */
  void* log_user;       /**< user pointer for @c log / 日志回调的用户指针 */
  srsp_str data_dir;    /**< may be {NULL,0}: schema/data root / 可为空：数据根目录覆盖 */
  uint32_t flags;       /**< SRSP_CTX_FLAG_* bitmask / 标志位掩码 */
} srsp_context_config;

/** @brief Subscription handle; 0 means invalid. @brief_zh 订阅句柄；0 表示无效。 */
typedef uint64_t srsp_subscription;

/**
 * @brief Returns the API version as (major << 16) | minor.
 * @brief_zh 返回 API 版本，编码为 (major << 16) | minor。
 */
SRSP_CORE_API uint32_t SRSP_CALL srsp_core_api_version(void);

/**
 * @brief Creates a kernel context; @c config may be NULL for defaults.
 * @brief_zh 创建内核上下文；config 可为 NULL 表示使用默认值。
 */
SRSP_CORE_API srsp_status SRSP_CALL srsp_context_create(
    const srsp_context_config* config, srsp_context** out, srsp_error* err);

/** @brief Destroys a context; idempotent for NULL. @brief_zh 销毁上下文；传 NULL 时无操作。 */
SRSP_CORE_API void SRSP_CALL srsp_context_destroy(srsp_context* ctx);

/**
 * @brief Loads a project (see docs/03-data-contracts.md section 8).
 * @brief_zh 加载工程（见 docs/03-data-contracts.md 第 8 节）。
 */
SRSP_CORE_API srsp_status SRSP_CALL srsp_load_project(srsp_context* ctx,
                                                    srsp_str path,
                                                    srsp_error* err);

/** @brief Unloads the current project. @brief_zh 卸载当前工程。 */
SRSP_CORE_API srsp_status SRSP_CALL srsp_unload_project(srsp_context* ctx,
                                                      srsp_error* err);

/**
 * @brief Loads a plugin directly by library path (M0 convenience).
 * @brief_zh 按库路径直接加载插件（M0 便利接口）。
 *
 * Project load resolves the plugin set from the project file in M1.
 * @zh M1 起由工程加载负责解析插件集合。
 */
SRSP_CORE_API srsp_status SRSP_CALL srsp_load_plugin(srsp_context* ctx,
                                                     srsp_str library_path,
                                                     srsp_error* err);

/** @brief Unloads the current plugin. @brief_zh 卸载当前插件。 */
SRSP_CORE_API srsp_status SRSP_CALL srsp_unload_plugin(srsp_context* ctx,
                                                       srsp_error* err);

/** @brief Starts the simulation clock. @brief_zh 启动仿真时钟。 */
SRSP_CORE_API srsp_status SRSP_CALL srsp_start(srsp_context* ctx, srsp_error* err);

/** @brief Pauses the simulation clock. @brief_zh 暂停仿真时钟。 */
SRSP_CORE_API srsp_status SRSP_CALL srsp_pause(srsp_context* ctx, srsp_error* err);

/**
 * @brief Advances exactly @p steps macro steps.
 * @brief_zh 精确推进 steps 个宏步。
 */
SRSP_CORE_API srsp_status SRSP_CALL srsp_step(srsp_context* ctx, uint64_t steps,
                                            srsp_error* err);

/** @brief Current simulation time in TAI nanoseconds. @brief_zh 当前仿真时间（TAI 纳秒）。 */
SRSP_CORE_API int64_t SRSP_CALL srsp_time_tai_ns(const srsp_context* ctx);

/**
 * @brief Copies an immutable snapshot of the state as Arrow IPC bytes.
 * @brief_zh 以 Arrow IPC 字节复制当前状态的不可变快照。
 *
 * Release the result with srsp_buffer_release.
 * @zh 返回值必须用 srsp_buffer_release 释放。
 */
SRSP_CORE_API srsp_status SRSP_CALL srsp_get_snapshot(srsp_context* ctx,
                                                    srsp_buffer* out,
                                                    srsp_error* err);

/** @brief Releases a buffer produced by the kernel. @brief_zh 释放内核返回的缓冲区。 */
SRSP_CORE_API void SRSP_CALL srsp_buffer_release(srsp_context* ctx,
                                               srsp_buffer* buffer);

/**
 * @brief Subscribes to a topic (docs/03 section 5); wildcards follow topic rules.
 * @brief_zh 订阅主题（docs/03 第 5 节）；通配符遵循主题规则。
 */
SRSP_CORE_API srsp_status SRSP_CALL srsp_subscribe(srsp_context* ctx,
                                                 srsp_str topic,
                                                 srsp_event_fn callback,
                                                 void* user,
                                                 srsp_subscription* out,
                                                 srsp_error* err);

/** @brief Removes a subscription. @brief_zh 取消订阅。 */
SRSP_CORE_API srsp_status SRSP_CALL srsp_unsubscribe(srsp_context* ctx,
                                                   srsp_subscription id,
                                                   srsp_error* err);

/**
 * @brief Generic capability passthrough.
 * @brief_zh 通用能力透传。
 *
 * Same method+payload semantics as the plugin ABI @c call and the sidecar
 * @c Invoke RPC.
 * @zh 与插件 ABI 的 call、边车 Invoke RPC 采用相同的 method + payload 语义。
 */
SRSP_CORE_API srsp_status SRSP_CALL srsp_plugin_call(srsp_context* ctx,
                                                   srsp_str plugin_id,
                                                   srsp_str method,
                                                   srsp_bytes request,
                                                   srsp_buffer* response,
                                                   srsp_error* err);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SRSP_CORE_API_H */
