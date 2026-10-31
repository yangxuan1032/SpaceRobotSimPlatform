/**
 * @file test_rhi_renderer.cpp
 * @brief Contract tests for renderer.rhi.
 *
 * The backend currently uses the QRhi Null implementation, which is public and
 * headless. D3D11 needs backend init params that Qt 6.11 does not expose in its
 * public headers (ISSUES P33); the test therefore asserts behaviour relative to
 * the Null backend without pretending a GPU device exists.
 */
#include <srsp/render/scene.hpp>

#include <string>

#include <QPoint>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>

#include "rhi_renderer.hpp"

using srsp::render::RhiRenderer;
using srsp::render::Viewport;

TEST_CASE("rhi renderer initialises on the null backend", "[render_rhi]") {
  RhiRenderer renderer;
  std::string error;
  REQUIRE(renderer.initialize(nullptr, Viewport{320, 180}, &error));
  CHECK((renderer.backend_name() == "d3d11" || renderer.backend_name() == "null"));
  CHECK(std::string(renderer.name()) == "rhi");

  const auto capabilities = renderer.capabilities();
  CHECK(capabilities.gpu == (renderer.backend_name() == "d3d11"));
  CHECK(capabilities.compute == false);
  CHECK(capabilities.max_viewports == 1);

  renderer.shutdown();
  CHECK(renderer.frame_count() == 0u);
}

TEST_CASE("rhi renderer renders frames offscreen", "[render_rhi]") {
  RhiRenderer renderer;
  REQUIRE(renderer.initialize(nullptr, Viewport{64, 48}, nullptr));
  REQUIRE(renderer.render_frame());
  REQUIRE(renderer.render_frame());
  CHECK(renderer.frame_count() == 2u);
  CHECK(renderer.resize(Viewport{128, 96}));
  REQUIRE(renderer.render_frame());
  CHECK(renderer.frame_count() == 3u);
  renderer.shutdown();
}

TEST_CASE("rhi renderer validates the viewport", "[render_rhi]") {
  RhiRenderer renderer;
  CHECK_FALSE(renderer.initialize(nullptr, Viewport{0, 0}, nullptr));
  REQUIRE(renderer.initialize(nullptr, Viewport{16, 16}, nullptr));
  CHECK_FALSE(renderer.resize(Viewport{-1, 16}));
  renderer.shutdown();
}

TEST_CASE("null backend does not claim a GPU or compute", "[render_rhi]") {
  RhiRenderer renderer;
  REQUIRE(renderer.initialize(nullptr, Viewport{32, 24}, nullptr));
  CHECK(renderer.capabilities().gpu == (renderer.backend_name() == "d3d11"));
  CHECK(srsp::render::rhi_backend_available());
  renderer.shutdown();
}
TEST_CASE("rhi renders the scene sphere on a GPU backend", "[render_rhi]") {
  RhiRenderer renderer;
  REQUIRE(renderer.initialize(nullptr, Viewport{256, 256}, nullptr));
  std::printf("rhi backend: %s\n", renderer.backend_name().c_str());
  REQUIRE(renderer.render_frame());

  if (renderer.backend_name() == "d3d11") {
    const QImage image = renderer.readback();
    REQUIRE_FALSE(image.isNull());
    CHECK(image.width() == 256);
    CHECK(image.height() == 256);

    // The corner is never covered by the triangle in clip space.
    int foreground = 0;
    for (int y = 0; y < image.height(); ++y) {
      for (int x = 0; x < image.width(); ++x) {
        const QColor pixel = image.pixelColor(x, y);
        if (pixel != QColor(8, 10, 16)) {
          ++foreground;
        }
      }
    }
    INFO("foreground=" << foreground);
    CHECK(foreground > 1000);
    const QColor corner = image.pixelColor(2, 2);
    CHECK(corner.red() == 8);
    CHECK(corner.green() == 10);
    CHECK(corner.blue() == 16);

    if (renderer.has_geometry()) {
      const QColor centre = image.pixelColor(128, 128);
      const int delta =
          std::abs(centre.red() - 8) + std::abs(centre.green() - 10) + std::abs(centre.blue() - 16);
      INFO("centre=" << centre.red() << "," << centre.green() << "," << centre.blue());
      CHECK(delta > 30);
    }
  }
  renderer.shutdown();
}
TEST_CASE("backend availability matches the backend actually created", "[render_rhi]") {
  RhiRenderer renderer;
  REQUIRE(renderer.initialize(nullptr, Viewport{32, 32}, nullptr));
  const bool gpu_backend = renderer.backend_name() == "d3d11";
  CHECK(srsp::render::rhi_backend_available() == gpu_backend);
  renderer.shutdown();
}
TEST_CASE("rhi renders scene boxes through the mesh path", "[render_rhi]") {
  RhiRenderer renderer;
  REQUIRE(renderer.initialize(nullptr, Viewport{256, 256}, nullptr));

  srsp::render::Scene scene{};
  srsp::render::Sphere first{};
  first.position_m[0] = -0.8;
  first.radius_m = 0.6;
  first.color[0] = 0.9f;
  first.color[1] = 0.3f;
  first.color[2] = 0.2f;
  first.mesh_uri = "box";
  first.orientation[3] = 1.0;
  scene.bodies.push_back(first);

  srsp::render::Sphere second = first;
  second.position_m[0] = 0.8;
  scene.bodies.push_back(second);
  renderer.set_scene(scene);
  REQUIRE(renderer.render_frame());

  if (renderer.backend_name() == "d3d11") {
    const QImage image = renderer.readback();
    REQUIRE_FALSE(image.isNull());
    int foreground = 0;
    for (int y = 0; y < image.height(); ++y) {
      for (int x = 0; x < image.width(); ++x) {
        if (image.pixelColor(x, y) != QColor(8, 10, 16)) {
          ++foreground;
        }
      }
    }
    INFO("foreground=" << foreground);
    CHECK(foreground > 500);
  }
  renderer.shutdown();
}
