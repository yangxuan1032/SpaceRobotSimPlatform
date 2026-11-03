/**
 * @file test_qt_mesh_dispatch.cpp
 * @brief mesh_uri must change the drawn geometry, not just the colour (O3).
 *
 * A box and a sphere at the same position and radius produce different images;
 * before the dispatch was wired both were drawn as ellipses and the images were
 * identical.
 */
#include <srsp/render/scene.hpp>

#include <string>

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

CameraView make_camera() {
  CameraView camera{};
  camera.eye[0] = 0.0;
  camera.eye[1] = -6.0;
  camera.eye[2] = 2.5;
  camera.target[0] = 0.0;
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

Scene make_scene(const char* mesh_uri) {
  Scene scene{};
  Sphere body{};
  body.radius_m = 1.0;
  body.color[0] = 0.9f;
  body.color[1] = 0.3f;
  body.color[2] = 0.2f;
  body.mesh_uri = mesh_uri;
  body.orientation[0] = 0.3826834;
  body.orientation[3] = 0.9238795;
  scene.bodies.push_back(body);
  return scene;
}

QImage render(const Scene& scene) {
  QtRenderer renderer;
  const Viewport viewport{320, 200};
  std::string error;
  REQUIRE(renderer.initialize(nullptr, viewport, &error));
  renderer.set_camera(make_camera());
  renderer.set_scene(scene);
  return renderer.render_image(viewport.width, viewport.height);
}

int differing_pixels(const QImage& a, const QImage& b) {
  int count = 0;
  for (int y = 0; y < a.height(); ++y) {
    for (int x = 0; x < a.width(); ++x) {
      if (a.pixel(x, y) != b.pixel(x, y)) {
        ++count;
      }
    }
  }
  return count;
}

}  // namespace

TEST_CASE("box and sphere meshes render differently", "[render_qt][mesh]") {
  const QImage box = render(make_scene("box"));
  const QImage sphere = render(make_scene(""));
  const int diff = differing_pixels(box, sphere);
  std::printf("mesh dispatch diff_pixels=%d\n", diff);
  CHECK(diff > 500);
}