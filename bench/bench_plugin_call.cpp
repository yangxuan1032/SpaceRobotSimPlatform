/**
 * @file bench_plugin_call.cpp
 * @brief Cost of one C ABI plugin call, so the per-step budget can be split.
 *
 * Prints mean, p50 and p95 in microseconds for `core.ping` on the hello plugin.
 * The plugin path comes from SRSP_HELLO_PLUGIN so CI can point at the built
 * module without a hard-coded tree layout.
 */
#include <srsp/core_api.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include <cstdio>
#include <cstdlib>

int main() {
  const char* plugin_path = std::getenv("SRSP_HELLO_PLUGIN");
  if (plugin_path == nullptr) {
    std::fprintf(stderr, "SRSP_HELLO_PLUGIN is not set\n");
    return 1;
  }

  srsp_error err{};
  srsp_context* ctx = nullptr;
  if (srsp_context_create(nullptr, &ctx, &err) != SRSP_OK) {
    std::fprintf(stderr, "context_create failed: %s\n", err.message != nullptr ? err.message : "?");
    return 1;
  }

  const std::string path(plugin_path);
  const srsp_str library{path.c_str(), path.size()};
  if (srsp_load_plugin(ctx, library, &err) != SRSP_OK) {
    std::fprintf(stderr, "load_plugin failed: %s\n", err.message != nullptr ? err.message : "?");
    srsp_context_destroy(ctx);
    return 1;
  }

  const std::string method = "core.ping";
  const srsp_str method_str{method.c_str(), method.size()};
  const std::string id = "org.srsp.hello";
  const srsp_str id_str{id.c_str(), id.size()};

  constexpr int kIterations = 20000;
  std::vector<double> samples;
  samples.reserve(kIterations);

  srsp_buffer response{};
  // Warm up so the first measurement does not include page faults.
  for (int i = 0; i < 100; ++i) {
    (void)srsp_plugin_call(ctx, id_str, method_str, srsp_bytes{nullptr, 0}, &response, &err);
    srsp_buffer_release(ctx, &response);
  }

  for (int i = 0; i < kIterations; ++i) {
    const auto start = std::chrono::steady_clock::now();
    const srsp_status status =
        srsp_plugin_call(ctx, id_str, method_str, srsp_bytes{nullptr, 0}, &response, &err);
    const auto stop = std::chrono::steady_clock::now();
    if (status != SRSP_OK) {
      std::fprintf(stderr, "plugin_call failed at %d\n", i);
      srsp_context_destroy(ctx);
      return 1;
    }
    srsp_buffer_release(ctx, &response);
    samples.push_back(std::chrono::duration<double, std::micro>(stop - start).count());
  }

  std::sort(samples.begin(), samples.end());
  double sum = 0.0;
  for (const double sample : samples) {
    sum += sample;
  }
  const double mean = sum / static_cast<double>(samples.size());
  const double p50 = samples[samples.size() / 2];
  const double p95 = samples[(samples.size() * 95) / 100];

  std::printf("plugin_calls=%d\n", kIterations);
  std::printf("plugin_call_mean_us=%.3f\n", mean);
  std::printf("plugin_call_p50_us=%.3f\n", p50);
  std::printf("plugin_call_p95_us=%.3f\n", p95);
  std::printf("plugin_budget_us=20 verdict=%s\n", p95 <= 20.0 ? "PASS" : "FAIL");
  srsp_context_destroy(ctx);
  return 0;
}