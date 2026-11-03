/**
 * @file test_qt_box.cpp
 * @brief Mesh and dual-origin verification for the Qt software backend (R3/R4).
 *
 * The box mesh exercises the mesh_uri path (procedural geometry) and the
 * dual-origin check shifts the camera and the body by 4e7 m: because the
 * projection subtracts the camera origin, the rendered image must be identical.
 */
#include <srsp/render/scene.hpp>

#include <QColor>
#include <QImage>
#include <catch2/catch_test_macros.hpp>

#include "qt_renderer.hpp"

using srsp::render::CameraView;
using srsp::render::Scene;
using srsp::render::Sphere;
using srsp::render::Viewport;
using srsp::studio::QtRenderer;

namespace {

CameraView make_camera(double shift_x) {
  CameraView camera{};
  camera.eye[0] = shift_x;
  camera.eye[1] = -6.0;
  camera.eye[2] = 0.0;
  camera.target[0] = shift_x;
  camera.target[1] = 0.0;
  camera.target[2] = 0.0;
  camera.up[0] = 0.0;
  camera.up[1] = 0.0;
  camera.up[2] = 1.0;
  camera.fov_deg = 45.0;
  camera.near_m = 0.01;
  camera.far_m = 1.0e9;
  return camera;
}

Scene make_scene(double shift_x) {
  Scene scene{};
  Sphere box{};
  box.position_m[0] = shift_x;
  box.position_m[1] = 0.0;
  box.position_m[2] = 0.0;
  box.radius_m = 1.0;
  box.color[0] = 0.9f;
  box.color[1] = 0.3f;
  box.color[2] = 0.2f;
  box.mesh_uri = "box";
  box.orientation[3] = 1.0;
  scene.bodies.push_back(box);
  return scene;
}

int count_foreground(const QImage& image) {
  const QColor background(8, 10, 16);
  int count = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (image.pixelColor(x, y) != background) {
        ++count;
      }
    }
  }
  return count;
}

}  // namespace

TEST_CASE("box mesh draws shaded geometry", "[render_qt]") {
  QtRenderer renderer;
  REQUIRE(renderer.initialize(nullptr, Viewport{256, 256}, nullptr));
  renderer.set_camera(make_camera(0.0));
  renderer.set_scene(make_scene(0.0));
  REQUIRE(renderer.render_frame());

  const QImage image = renderer.render_image(256, 256);
  REQUIRE_FALSE(image.isNull());
  const int foreground = count_foreground(image);
  INFO("foreground pixels: " << foreground);
  CHECK(foreground > 1000);  // the box covers a large part of the view
  CHECK(image.pixelColor(2, 2) == QColor(8, 10, 16));
}

TEST_CASE("dual origin keeps the image identical at 4e7 m", "[render_qt]") {
  QtRenderer renderer;
  REQUIRE(renderer.initialize(nullptr, Viewport{256, 256}, nullptr));

  renderer.set_camera(make_camera(0.0));
  renderer.set_scene(make_scene(0.0));
  REQUIRE(renderer.render_frame());
  const QImage local = renderer.render_image(256, 256);

  const double shift = 4.0e7;
  renderer.set_camera(make_camera(shift));
  renderer.set_scene(make_scene(shift));
  REQUIRE(renderer.render_frame());
  const QImage shifted = renderer.render_image(256, 256);

  REQUIRE(local.size() == shifted.size());
  CHECK(local == shifted);
}
