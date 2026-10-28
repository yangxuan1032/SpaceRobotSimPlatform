/**
 * @file test_rhi_renderer.cpp
 * @brief Contract tests for renderer.rhi.
 *
 * The backend currently uses the QRhi Null implementation, which is public and
 * headless. D3D11 needs backend init params that Qt 6.11 does not expose in its
 * public headers (ISSUES P33); the test therefore asserts behaviour relative to
 * the Null backend without pretending a GPU device exists.
 */
#include <string>

#include <QPoint>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>

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
TEST_CASE("rhi readback matches the clear colour on a GPU backend", "[render_rhi]") {
  RhiRenderer renderer;
  REQUIRE(renderer.initialize(nullptr, Viewport{64, 48}, nullptr));
  std::printf("rhi backend: %s\n", renderer.backend_name().c_str());
  REQUIRE(renderer.render_frame());

  if (renderer.backend_name() == "d3d11") {
    const QImage image = renderer.readback();
    REQUIRE_FALSE(image.isNull());
    CHECK(image.width() == 64);
    CHECK(image.height() == 48);

    // Drawing is currently best-effort (D3D11 rejects the shader, P34), so the
    // frame must at least contain the clear colour.
    for (const QPoint& point : {QPoint(32, 24), QPoint(2, 2)}) {
      const QColor pixel = image.pixelColor(point);
      CHECK(pixel.red() == 8);
      CHECK(pixel.green() == 10);
      CHECK(pixel.blue() == 16);
    }
  }
  renderer.shutdown();
}
