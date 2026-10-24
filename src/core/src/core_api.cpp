/**
 * @file core_api.cpp
 * @brief Implementation of the host-side C API (srsp/core_api.h).
 *
 * The context owns the plugin host, the fixed-step clock, the scheduler and
 * the diagnostics sink. All memory handed back to the caller is allocated with
 * the context allocator and freed by srsp_buffer_release.
 */
#include <srsp/core_api.h>

#include <srsp/core/diagnostics.hpp>
#include <srsp/core/plugin_host.hpp>
#include <srsp/core/scheduler.hpp>
#include <srsp/core/time_service.hpp>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

#if defined(_WIN32)
#  include <malloc.h>
#endif

struct srsp_context {
  srsp_context_config config{};
  srsp::core::TimeService clock{1'000'000, 0};
  srsp::core::Diagnostics diagnostics;
  srsp::core::Scheduler scheduler{clock, &diagnostics};
  std::unique_ptr<srsp::core::PluginHost> host;
  bool plugin_loaded = false;
};

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

void SRSP_CALL ctx_log(void* user, int32_t level, srsp_str message) {
  auto* ctx = static_cast<srsp_context*>(user);
  if (ctx != nullptr && ctx->config.log != nullptr) {
    ctx->config.log(ctx->config.log_user, level, message);
  }
}

double SRSP_CALL ctx_time(void* user) {
  auto* ctx = static_cast<srsp_context*>(user);
  return ctx != nullptr ? static_cast<double>(ctx->clock.now_ns()) * 1e-9 : 0.0;
}

int64_t SRSP_CALL ctx_time_ns(void* user) {
  auto* ctx = static_cast<srsp_context*>(user);
  return ctx != nullptr ? ctx->clock.now_ns() : 0;
}

srsp_status SRSP_CALL ctx_alloc(void*, uint64_t size, uint64_t alignment,
                                void** out) {
  if (out == nullptr || size == 0) {
    return SRSP_E_INVALID_ARGUMENT;
  }
  if (alignment < alignof(void*)) {
    alignment = alignof(void*);
  }
#if defined(_WIN32)
  *out = _aligned_malloc(static_cast<size_t>(size), static_cast<size_t>(alignment));
#else
  if (posix_memalign(out, static_cast<size_t>(alignment),
                     static_cast<size_t>(size)) != 0) {
    *out = nullptr;
  }
#endif
  return *out != nullptr ? SRSP_OK : SRSP_E_OUT_OF_MEMORY;
}

void SRSP_CALL ctx_free(void*, void* ptr) {
#if defined(_WIN32)
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

srsp_status SRSP_CALL ctx_publish(void*, srsp_str, srsp_bytes, uint64_t) {
  return SRSP_E_UNSUPPORTED;
}

srsp_status SRSP_CALL ctx_shm_map(void*, srsp_str, uint64_t, int32_t, void**) {
  return SRSP_E_UNSUPPORTED;
}

void SRSP_CALL ctx_shm_unmap(void*, void*, uint64_t) {}

srsp_host_v1 make_services() {
  srsp_host_v1 services{};
  services.struct_size = sizeof(srsp_host_v1);
  services.log = ctx_log;
  services.time_tai = ctx_time;
  services.time_tai_ns = ctx_time_ns;
  services.alloc = ctx_alloc;
  services.free = ctx_free;
  services.publish = ctx_publish;
  services.shm_map = ctx_shm_map;
  services.shm_unmap = ctx_shm_unmap;
  return services;
}

}  // namespace

extern "C" {

SRSP_CORE_API uint32_t SRSP_CALL srsp_core_api_version(void) {
  return (SRSP_CORE_API_MAJOR << 16) | SRSP_CORE_API_MINOR;
}

SRSP_CORE_API srsp_status SRSP_CALL srsp_context_create(
    const srsp_context_config* config, srsp_context** out, srsp_error* err) {
  if (out == nullptr) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "out context is null");
    return SRSP_E_INVALID_ARGUMENT;
  }
  *out = nullptr;

  auto* ctx = new (std::nothrow) srsp_context();
  if (ctx == nullptr) {
    set_error(err, SRSP_E_OUT_OF_MEMORY, "context allocation failed");
    return SRSP_E_OUT_OF_MEMORY;
  }
  if (config != nullptr) {
    ctx->config = *config;
  } else {
    ctx->config.struct_size = sizeof(srsp_context_config);
  }
  ctx->host = std::make_unique<srsp::core::PluginHost>(make_services());
  *out = ctx;
  return SRSP_OK;
}

SRSP_CORE_API void SRSP_CALL srsp_context_destroy(srsp_context* ctx) {
  delete ctx;
}

SRSP_CORE_API srsp_status SRSP_CALL srsp_load_project(srsp_context* ctx,
                                                      srsp_str path,
                                                      srsp_error* err) {
  (void)ctx;
  (void)path;
  set_error(err, SRSP_E_UNSUPPORTED, "project loading lands in M1");
  return SRSP_E_UNSUPPORTED;
}

SRSP_CORE_API srsp_status SRSP_CALL srsp_unload_project(srsp_context* ctx,
                                                        srsp_error* err) {
  (void)ctx;
  set_error(err, SRSP_E_UNSUPPORTED, "project loading lands in M1");
  return SRSP_E_UNSUPPORTED;
}

SRSP_CORE_API srsp_status SRSP_CALL srsp_load_plugin(srsp_context* ctx,
                                                     srsp_str library_path,
                                                     srsp_error* err) {
  if (ctx == nullptr) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "context is null");
    return SRSP_E_INVALID_ARGUMENT;
  }
  if (ctx->plugin_loaded) {
    set_error(err, SRSP_E_INVALID_STATE, "a plugin is already loaded");
    return SRSP_E_INVALID_STATE;
  }
  srsp_status status = ctx->host->load(
      std::string_view(library_path.data != nullptr ? library_path.data : "",
                       library_path.size),
      err);
  if (status != SRSP_OK) {
    return status;
  }
  status = ctx->host->init(srsp_str{}, err);
  if (status != SRSP_OK) {
    ctx->host->unload();
    return status;
  }
  ctx->plugin_loaded = true;
  return SRSP_OK;
}

SRSP_CORE_API srsp_status SRSP_CALL srsp_unload_plugin(srsp_context* ctx,
                                                       srsp_error* err) {
  if (ctx == nullptr) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "context is null");
    return SRSP_E_INVALID_ARGUMENT;
  }
  if (!ctx->plugin_loaded) {
    set_error(err, SRSP_E_NOT_READY, "no plugin is loaded");
    return SRSP_E_NOT_READY;
  }
  ctx->host->unload();
  ctx->plugin_loaded = false;
  return SRSP_OK;
}

SRSP_CORE_API srsp_status SRSP_CALL srsp_start(srsp_context* ctx, srsp_error* err) {
  if (ctx == nullptr) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "context is null");
    return SRSP_E_INVALID_ARGUMENT;
  }
  ctx->clock.resume();
  return SRSP_OK;
}

SRSP_CORE_API srsp_status SRSP_CALL srsp_pause(srsp_context* ctx, srsp_error* err) {
  if (ctx == nullptr) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "context is null");
    return SRSP_E_INVALID_ARGUMENT;
  }
  ctx->clock.pause();
  return SRSP_OK;
}

SRSP_CORE_API srsp_status SRSP_CALL srsp_step(srsp_context* ctx, uint64_t steps,
                                              srsp_error* err) {
  if (ctx == nullptr) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "context is null");
    return SRSP_E_INVALID_ARGUMENT;
  }
  if (!ctx->plugin_loaded) {
    set_error(err, SRSP_E_NOT_READY, "no plugin is loaded");
    return SRSP_E_NOT_READY;
  }
  const uint64_t executed = ctx->scheduler.run(steps, nullptr);
  if (executed != steps) {
    set_error(err, SRSP_E_NOT_READY, "clock is paused");
    return SRSP_E_NOT_READY;
  }
  return SRSP_OK;
}

SRSP_CORE_API int64_t SRSP_CALL srsp_time_tai_ns(const srsp_context* ctx) {
  return ctx != nullptr ? ctx->clock.now_ns() : 0;
}

SRSP_CORE_API srsp_status SRSP_CALL srsp_get_snapshot(srsp_context* ctx,
                                                      srsp_buffer* out,
                                                      srsp_error* err) {
  (void)ctx;
  (void)out;
  set_error(err, SRSP_E_UNSUPPORTED,
            "Arrow snapshots are serialized in M1 (core_api 0.2)");
  return SRSP_E_UNSUPPORTED;
}

SRSP_CORE_API void SRSP_CALL srsp_buffer_release(srsp_context* ctx,
                                                 srsp_buffer* buffer) {
  if (ctx == nullptr || buffer == nullptr) {
    return;
  }
  ctx_free(nullptr, buffer->data);
  buffer->data = nullptr;
  buffer->size = 0;
}

SRSP_CORE_API srsp_status SRSP_CALL srsp_subscribe(srsp_context* ctx,
                                                   srsp_str topic,
                                                   srsp_event_fn callback,
                                                   void* user,
                                                   srsp_subscription* out,
                                                   srsp_error* err) {
  (void)ctx;
  (void)topic;
  (void)callback;
  (void)user;
  (void)out;
  set_error(err, SRSP_E_UNSUPPORTED, "subscriptions land in M1");
  return SRSP_E_UNSUPPORTED;
}

SRSP_CORE_API srsp_status SRSP_CALL srsp_unsubscribe(srsp_context* ctx,
                                                     srsp_subscription id,
                                                     srsp_error* err) {
  (void)ctx;
  (void)id;
  set_error(err, SRSP_E_UNSUPPORTED, "subscriptions land in M1");
  return SRSP_E_UNSUPPORTED;
}

SRSP_CORE_API srsp_status SRSP_CALL srsp_plugin_call(srsp_context* ctx,
                                                     srsp_str plugin_id,
                                                     srsp_str method,
                                                     srsp_bytes request,
                                                     srsp_buffer* response,
                                                     srsp_error* err) {
  (void)plugin_id;  // M0: a single loaded plugin; multi-plugin routing in M1.
  if (ctx == nullptr) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "context is null");
    return SRSP_E_INVALID_ARGUMENT;
  }
  if (!ctx->plugin_loaded) {
    set_error(err, SRSP_E_NOT_READY, "no plugin is loaded");
    return SRSP_E_NOT_READY;
  }
  return ctx->host->call(method, request, response, err);
}

}  // extern "C"
