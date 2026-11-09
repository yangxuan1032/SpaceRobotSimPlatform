/**
 * @file test_capability_matrix.cpp
 * @brief A2: the capability matrix in docs/11 must be backed by behaviour.
 *
 * Every claim the matrix makes about a backend is asserted here, so removing a
 * backend feature (or mislabelling it) fails a test instead of leaving a stale
 * table behind.
 */
#include <srsp/render/renderer.hpp>
#include <srsp/render/scene.hpp>

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "qt_renderer.hpp"
#include "rhi_renderer.hpp"

using srsp::render::make_null_renderer;
using srsp::render::RendererCapabilities;
using srsp::render::Scene;
using srsp::render::Viewport;
using srsp::studio::QtRenderer;

namespace {

void check_common(const RendererCapabilities& caps) {
  CHECK(caps.backend != nullptr);
  CHECK(std::string(caps.backend).size() > 0u);
  // No backend claims compute yet; the matrix has no compute row.
  CHECK_FALSE(caps.compute);
  CHECK(caps.max_viewports >= 1);
}

}  // namespace

TEST_CASE("null backend claims headless only", "[capability]") {
  const auto renderer = make_null_renderer();
  REQUIRE(renderer != nullptr);
  check_common(renderer->capabilities());
  CHECK(std::string(renderer->capabilities().backend) == "null");
  CHECK_FALSE(renderer->capabilities().gpu);
}

TEST_CASE("qt backend is software, never GPU", "[capability]") {
  QtRenderer renderer;
  const Viewport viewport{160, 120};
  std::string error;
  REQUIRE(renderer.initialize(nullptr, viewport, &error));
  check_common(renderer.capabilities());
  CHECK(std::string(renderer.name()) == "qt");
  CHECK(std::string(renderer.capabilities().backend) == "qt");
  CHECK_FALSE(renderer.capabilities().gpu);
  renderer.shutdown();
}

TEST_CASE("rhi backend reports GPU only on a real device", "[capability]") {
  srsp::render::RhiRenderer renderer;
  const Viewport viewport{160, 120};
  std::string error;
  if (!renderer.initialize(nullptr, viewport, &error)) {
    SKIP("RHI backend unavailable: " << error);
  }
  check_common(renderer.capabilities());
  const std::string backend = renderer.backend_name();
  const bool is_gpu = backend == "d3d11";
  CHECK(renderer.capabilities().gpu == is_gpu);
  // Geometry needs a device; the Null fallback must not pretend to draw.
  CHECK(renderer.has_geometry() == is_gpu);
  renderer.shutdown();
}

TEST_CASE("an empty scene still renders one frame", "[capability]") {
  QtRenderer renderer;
  const Viewport viewport{64, 48};
  std::string error;
  REQUIRE(renderer.initialize(nullptr, viewport, &error));
  Scene empty{};
  renderer.set_scene(empty);
  CHECK(renderer.render_frame());
  CHECK(renderer.frame_count() == 1u);
  renderer.shutdown();
}
