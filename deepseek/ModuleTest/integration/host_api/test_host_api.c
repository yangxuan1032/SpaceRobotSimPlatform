/**
 * @file test_host_api.c
 * @brief Proves that core_api.h is usable from plain C (no C++ required).
 *
 * Usage: test_host_api <plugin-path>
 * Exit code 0 on success; non-zero with a message on the first failure.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <srsp/core_api.h>

static int fail(const char* what) {
  fprintf(stderr, "FAIL: %s\n", what);
  return 1;
}

static srsp_str make_str(const char* text) {
  srsp_str value;
  value.data = text;
  value.size = strlen(text);
  return value;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: test_host_api <plugin-path>\n");
    return 2;
  }

  if (srsp_core_api_version() != ((SRSP_CORE_API_MAJOR << 16) | SRSP_CORE_API_MINOR)) {
    return fail("core api version mismatch");
  }

  srsp_context_config config;
  memset(&config, 0, sizeof(config));
  config.struct_size = sizeof(config);
  config.flags = SRSP_CTX_FLAG_HEADLESS;

  srsp_error err;
  memset(&err, 0, sizeof(err));
  srsp_context* ctx = NULL;
  if (srsp_context_create(&config, &ctx, &err) != SRSP_OK || ctx == NULL) {
    return fail("context create failed");
  }

  /* Calling before a plugin is loaded must be rejected, not crash. */
  {
    srsp_buffer none;
    memset(&none, 0, sizeof(none));
    if (srsp_plugin_call(ctx, make_str(""), make_str("core.ping"), (srsp_bytes){0},
                         &none, &err) != SRSP_E_NOT_READY) {
      srsp_context_destroy(ctx);
      return fail("call before load was not rejected");
    }
  }

  if (srsp_load_plugin(ctx, make_str(argv[1]), &err) != SRSP_OK) {
    srsp_context_destroy(ctx);
    return fail("load plugin failed");
  }

  srsp_buffer response;
  memset(&response, 0, sizeof(response));
  if (srsp_plugin_call(ctx, make_str(""), make_str("core.ping"), (srsp_bytes){0},
                       &response, &err) != SRSP_OK) {
    srsp_context_destroy(ctx);
    return fail("plugin call failed");
  }
  if (response.data == NULL || response.size == 0) {
    srsp_context_destroy(ctx);
    return fail("empty response");
  }
  srsp_buffer_release(ctx, &response);
  if (response.data != NULL || response.size != 0) {
    srsp_context_destroy(ctx);
    return fail("buffer release did not clear the buffer");
  }

  if (srsp_step(ctx, 3, &err) != SRSP_OK) {
    srsp_context_destroy(ctx);
    return fail("step failed");
  }
  if (srsp_time_tai_ns(ctx) != 3 * 1000000) {
    srsp_context_destroy(ctx);
    return fail("clock did not advance by 3 macro steps");
  }

  srsp_pause(ctx, &err);
  if (srsp_step(ctx, 1, &err) != SRSP_E_NOT_READY) {
    srsp_context_destroy(ctx);
    return fail("step while paused was not rejected");
  }

  if (srsp_unload_plugin(ctx, &err) != SRSP_OK) {
    srsp_context_destroy(ctx);
    return fail("unload plugin failed");
  }

  srsp_context_destroy(ctx);
  printf("PASS: core_api works from C\n");
  return 0;
}
