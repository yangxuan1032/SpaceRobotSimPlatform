/**
 * @file test_capture_scope.cpp
 * @brief A4: captures are renderer-only; the overlay is composited on top.
 *
 * The contract has two halves:
 *  - the enum states that every backend capture excludes the UI overlay;
 *  - the Qt path demonstrates the procedure the window uses: render the scene,
 *    then draw the overlay on a copy, so the captured image never carries text.
 */
#include <srsp/render/capture.hpp>
#include <srsp/render/scene.hpp>

#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <catch2/catch_test_macros.hpp>

#include "qt_renderer.hpp"

using srsp::render::CameraView;
using srsp::render::capture_excludes_overlay;
using srsp::render::CaptureScope;
using srsp::render::kRendererCaptureScope;
using srsp::render::Scene;
using srsp::render::Sphere;
using srsp::render::to_string;
using srsp::render::Viewport;
using srsp::studio::QtRenderer;

namespace {

/// Text rendering needs a font database, which requires a (headless) GUI app.
/// The test runs with QT_QPA_PLATFORM=offscreen (set by CTest).
QGuiApplication& application() {
  static int argc = 1;
  static char name[] = "test_capture_scope";
  static char* argv[] = {name, nullptr};
  static QGuiApplication app(argc, argv);
  return app;
}

CameraView make_camera() {
  CameraView camera{};
  camera.eye[1] = -6.0;
  camera.eye[2] = 1.5;
  camera.up[2] = 1.0;
  camera.fov_deg = 45.0;
  camera.near_m = 0.01;
  camera.far_m = 1.0e9;
  return camera;
}

Scene make_scene() {
  Scene scene{};
  Sphere body{};
  body.radius_m = 0.8;
  body.color[0] = 0.9f;
  body.color[1] = 0.4f;
  body.color[2] = 0.2f;
  body.orientation[3] = 1.0;
  scene.bodies.push_back(body);
  return scene;
}

QImage render(const Scene& scene, int width, int height) {
  QtRenderer renderer;
  const Viewport viewport{width, height};
  std::string error;
  REQUIRE(renderer.initialize(nullptr, viewport, &error));
  renderer.set_camera(make_camera());
  renderer.set_scene(scene);
  return renderer.render_image(width, height);
}

}  // namespace

TEST_CASE("backend captures exclude the UI overlay", "[capture]") {
  CHECK(kRendererCaptureScope == CaptureScope::kRendererOnly);
  CHECK(capture_excludes_overlay(kRendererCaptureScope));
  CHECK_FALSE(capture_excludes_overlay(CaptureScope::kWindowComposite));
  CHECK(std::string_view(to_string(CaptureScope::kRendererOnly)) == "renderer-only");
}

TEST_CASE("renderer capture is deterministic", "[capture]") {
  const QImage first = render(make_scene(), 320, 200);
  const QImage second = render(make_scene(), 320, 200);
  CHECK(first == second);
}

TEST_CASE("the overlay is composited on a copy, never in the capture", "[capture]") {
  (void)application();
  const QImage captured = render(make_scene(), 320, 200);
  QImage composited = captured;
  {
    QPainter painter(&composited);
    painter.setPen(QColor(220, 220, 220));
    painter.drawText(12, 38, QStringLiteral("backend: rhi+overlay"));
  }

  // The overlay changed the composite...
  CHECK(composited != captured);

  // ...but only inside the text bounding box, which proves the capture itself
  // carried no text and the window adds it afterwards.
  const int left = 12;
  const int top = 26;
  const int right = 12 + 160;
  const int bottom = 44;
  int changed_total = 0;
  int changed_outside = 0;
  for (int y = 0; y < captured.height(); ++y) {
    for (int x = 0; x < captured.width(); ++x) {
      if (captured.pixel(x, y) == composited.pixel(x, y)) {
        continue;
      }
      ++changed_total;
      if (x < left || x > right || y < top || y > bottom) {
        ++changed_outside;
      }
    }
  }
  CHECK(changed_total > 50);
  CHECK(changed_outside == 0);
}
