/**
 * @file test_subscription.cpp
 * @brief B2: subscribe / publish / unsubscribe through the public C API.
 *
 * The hello plugin publishes "hello.tick" on every controller.update, so the
 * test can drive a real publish path instead of calling the host table.
 */
#include <srsp/core_api.h>

#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>

namespace {

uint64_t g_last_size = 0;
uint64_t g_last_schema = 0;

void SRSP_CALL on_event(void* user, srsp_str /*topic*/, srsp_bytes payload, uint64_t schema_hash) {
  auto* counter = static_cast<int*>(user);
  if (counter != nullptr) {
    ++(*counter);
  }
  g_last_size = payload.size;
  g_last_schema = schema_hash;
}

bool call_update(srsp_context* ctx, srsp_error* err) {
  srsp_buffer response{};
  const srsp_status status = srsp_plugin_call(ctx, SRSP_STR_LITERAL("org.srsp.examples.hello"),
                                              SRSP_STR_LITERAL("controller.update"),
                                              srsp_bytes{nullptr, 0}, &response, err);
  srsp_buffer_release(ctx, &response);
  return status == SRSP_OK;
}

}  // namespace

TEST_CASE("subscription receives published events until unsubscribed", "[subscription]") {
  const char* plugin_path = std::getenv("SRSP_TEST_PLUGIN");
  REQUIRE(plugin_path != nullptr);

  srsp_context_config config{};
  config.struct_size = sizeof(srsp_context_config);
  srsp_context* ctx = nullptr;
  srsp_error err{};
  REQUIRE(srsp_context_create(&config, &ctx, &err) == SRSP_OK);
  REQUIRE(srsp_load_plugin(ctx, srsp_str{plugin_path, std::strlen(plugin_path)}, &err) == SRSP_OK);

  int received = 0;
  srsp_subscription sub = 0;
  REQUIRE(srsp_subscribe(ctx, SRSP_STR_LITERAL("hello.tick"), &on_event, &received, &sub, &err) ==
          SRSP_OK);
  CHECK(sub != 0);

  REQUIRE(call_update(ctx, &err));
  CHECK(received == 1);
  CHECK(g_last_size == 4u);
  CHECK(g_last_schema == 0u);

  // An unrelated topic stays silent even while events flow.
  int other = 0;
  srsp_subscription other_sub = 0;
  REQUIRE(srsp_subscribe(ctx, SRSP_STR_LITERAL("no.such.topic"), &on_event, &other, &other_sub,
                         &err) == SRSP_OK);
  REQUIRE(call_update(ctx, &err));
  CHECK(received == 2);
  CHECK(other == 0);

  // Unsubscribe stops delivery, and doing it twice is not an error.
  REQUIRE(srsp_unsubscribe(ctx, sub, &err) == SRSP_OK);
  REQUIRE(call_update(ctx, &err));
  CHECK(received == 2);
  CHECK(srsp_unsubscribe(ctx, sub, &err) == SRSP_OK);
  CHECK(srsp_unsubscribe(ctx, 999999, &err) == SRSP_OK);

  // Invalid arguments are rejected instead of silently succeeding.
  srsp_subscription bad = 0;
  CHECK(srsp_subscribe(ctx, SRSP_STR_LITERAL("hello.tick"), nullptr, nullptr, &bad, &err) ==
        SRSP_E_INVALID_ARGUMENT);
  CHECK(srsp_subscribe(ctx, srsp_str{nullptr, 0}, &on_event, nullptr, &bad, &err) ==
        SRSP_E_INVALID_ARGUMENT);

  REQUIRE(srsp_unsubscribe(ctx, other_sub, &err) == SRSP_OK);
  srsp_context_destroy(ctx);
}
