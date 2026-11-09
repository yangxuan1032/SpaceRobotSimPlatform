/**
 * @file test_rhi_text.cpp
 * @brief B12(b): the RHI backend draws overlay text inside its own capture.
 *
 * On the Null backend there is no texture support, so the test only proves the
 * renderer still works; on D3D11 it renders "SRSP" and asserts bright pixels in
 * the text region of the readback (A4: captures are renderer-only, so text drawn
 * by the pipeline must appear there).
 */
#include <srsp/render/capture.hpp>
#include <srsp/render/scene.hpp>

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "rhi_renderer.hpp"

using srsp::render::CameraView;
using srsp::render::RhiRenderer;
using srsp::render::Scene;
using srsp::render::Viewport;

TEST_CASE("RHI renders overlay text into the capture", "[render_rhi][text]") {
  RhiRenderer renderer;
  const Viewport viewport{320, 200};
  std::string error;
  if (!renderer.initialize(nullptr, viewport, &error)) {
    SKIP("RHI backend unavailable: " << error);
  }
  CameraView camera{};
  camera.eye[1] = -6.0;
  camera.eye[2] = 1.5;
  camera.up[2] = 1.0;
  camera.fov_deg = 45.0;
  camera.far_m = 1.0e9;
  renderer.set_camera(camera);

  Scene scene{};
  renderer.set_scene(scene);
  renderer.set_overlay_text("SRSP");
  REQUIRE(renderer.resize(viewport));
  REQUIRE(renderer.render_frame(true));

  const std::string backend = renderer.backend_name();
  if (backend != "d3d11") {
    CHECK_FALSE(renderer.has_text_pipeline());
    SKIP("no GPU backend: text pipeline is D3D11-only, backend=" << backend);
  }

  REQUIRE(renderer.has_text_pipeline());
  const QImage image = renderer.readback();
  REQUIRE(!image.isNull());

  // Count bright pixels inside the text region (12,12 with scale 3 → ~4 glyphs).
  int bright = 0;
  for (int y = 10; y < 40; ++y) {
    for (int x = 10; x < 100; ++x) {
      const QRgb pixel = image.pixel(x, y);
      if (qRed(pixel) > 100 && qGreen(pixel) > 100 && qBlue(pixel) > 100) {
        ++bright;
      }
    }
  }
  std::printf("rhi text bright_pixels=%d backend=%s\n", bright, backend.c_str());
  CHECK(bright > 20);
}
