/**
 * @file bench_rhi.cpp
 * @brief Measures renderer.rhi frame time at 1920x1080 (S4).
 *
 * Two modes: "present" (draw only, no readback) and "capture" (draw plus the
 * GPU-to-CPU copy). The copy is synchronous and dominates, so both numbers are
 * reported and only "present" is compared with the 30 fps entry criterion.
 */
#include <srsp/render/renderer.hpp>
#include <srsp/render/scene.hpp>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include <cstdio>
#include <rhi_renderer.hpp>

namespace {

double percentile(std::vector<double> samples, double fraction) {
  if (samples.empty()) {
    return 0.0;
  }
  std::sort(samples.begin(), samples.end());
  const auto index = static_cast<std::size_t>(fraction * (samples.size() - 1));
  return samples[index];
}

}  // namespace

int main(int argc, char** argv) {
  const int frames = argc > 1 ? std::stoi(argv[1]) : 120;
  const srsp::render::Viewport viewport{1920, 1080};

  srsp::render::RhiRenderer renderer;
  std::string error;
  if (!renderer.initialize(nullptr, viewport, &error)) {
    std::fprintf(stderr, "initialize failed: %s\n", error.c_str());
    return 1;
  }
  std::printf("backend=%s resolution=%dx%d frames=%d\n", renderer.backend_name().c_str(),
              viewport.width, viewport.height, frames);

  srsp::render::CameraView camera{};
  camera.eye[1] = -5.0;
  camera.up[2] = 1.0;
  camera.fov_deg = 45.0;
  camera.near_m = 0.01;
  camera.far_m = 1.0e9;
  renderer.set_camera(camera);

  for (const bool capture : {false, true}) {
    std::vector<double> samples_ms;
    samples_ms.reserve(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i) {
      const auto start = std::chrono::steady_clock::now();
      renderer.render_frame(capture);
      if (!capture) {
        renderer.wait_idle();  // measure GPU completion, not just submission
      }
      const auto end = std::chrono::steady_clock::now();
      samples_ms.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }
    const double mean = [&] {
      double sum = 0.0;
      for (const double value : samples_ms) {
        sum += value;
      }
      return sum / static_cast<double>(samples_ms.size());
    }();
    std::printf("%s mean=%.2fms p50=%.2fms p95=%.2fms fps=%.1f\n", capture ? "capture" : "present",
                mean, percentile(samples_ms, 0.50), percentile(samples_ms, 0.95), 1000.0 / mean);
  }

  renderer.shutdown();
  return 0;
}