/**
 * @file test_projection.cpp
 * @brief Unit tests for the dual-origin pinhole projection.
 */
#include <srsp/render/projection.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace srsp::render;

namespace {

CameraView make_camera() {
  CameraView camera;
  camera.eye[0] = 0.0;
  camera.eye[1] = 0.0;
  camera.eye[2] = 10.0;
  camera.target[0] = 0.0;
  camera.target[1] = 0.0;
  camera.target[2] = 0.0;
  camera.up[0] = 0.0;
  camera.up[1] = 1.0;
  camera.up[2] = 0.0;
  camera.fov_deg = 90.0;
  camera.near_m = 0.1;
  camera.far_m = 1.0e9;
  return camera;
}

}  // namespace

TEST_CASE("projection puts the target at the viewport centre", "[projection]") {
  const CameraView camera = make_camera();
  const Viewport viewport{800, 600};
  const double origin[3] = {0.0, 0.0, 0.0};
  const double point[3] = {0.0, 0.0, 0.0};

  const ScreenPoint projected = project(camera, viewport, point, origin);
  REQUIRE(projected.visible);
  CHECK(projected.x == Approx(400.0).margin(1e-9));
  CHECK(projected.y == Approx(300.0).margin(1e-9));
  CHECK(projected.depth == Approx(10.0).epsilon(1e-12));
}

TEST_CASE("projection scales with distance and field of view", "[projection]") {
  CameraView camera = make_camera();
  const Viewport viewport{800, 600};
  const double origin[3] = {0.0, 0.0, 0.0};
  const double point[3] = {1.0, 0.0, 0.0};

  const ScreenPoint projected = project(camera, viewport, point, origin);
  REQUIRE(projected.visible);
  // fov 90 deg, height 600 -> focal = 300 px; x offset = 1 m * 300 / 10 m.
  CHECK(projected.x == Approx(400.0 + 30.0).margin(1e-9));
  CHECK(projected.y == Approx(300.0).margin(1e-9));
}

TEST_CASE("projection rejects points behind the near plane", "[projection]") {
  const CameraView camera = make_camera();
  const Viewport viewport{800, 600};
  const double origin[3] = {0.0, 0.0, 0.0};
  const double behind[3] = {0.0, 0.0, 20.0};  // behind the camera
  CHECK_FALSE(project(camera, viewport, behind, origin).visible);
}

TEST_CASE("dual origin keeps precision at orbital distances", "[projection]") {
  CameraView camera = make_camera();
  const Viewport viewport{1280, 720};

  const double local_origin[3] = {0.0, 0.0, 0.0};
  const double local_point[3] = {1.0, 0.0, 0.0};
  const ScreenPoint local = project(camera, viewport, local_point, local_origin);

  // Shift camera, point and origin by 4e7 m: the projection must not change.
  const double shift[3] = {4.0e7, -3.0e7, 2.0e7};
  CameraView shifted = camera;
  for (int i = 0; i < 3; ++i) {
    shifted.eye[i] += shift[i];
    shifted.target[i] += shift[i];
  }
  const double shifted_point[3] = {shift[0] + 1.0, shift[1], shift[2]};
  const ScreenPoint far = project(shifted, viewport, shifted_point, shift);

  REQUIRE(local.visible);
  REQUIRE(far.visible);
  CHECK(far.x == Approx(local.x).margin(1e-6));
  CHECK(far.y == Approx(local.y).margin(1e-6));
  CHECK(far.depth == Approx(local.depth).margin(1e-6));
}

TEST_CASE("projection vector helpers behave", "[projection]") {
  double a[3] = {1.0, 0.0, 0.0};
  double b[3] = {0.0, 1.0, 0.0};
  double out[3] = {0.0, 0.0, 0.0};
  cross3(a, b, out);
  CHECK(out[2] == Approx(1.0).margin(1e-12));
  CHECK(dot3(a, b) == Approx(0.0).margin(1e-12));

  double zero[3] = {0.0, 0.0, 0.0};
  CHECK_FALSE(normalize3(zero));
  double unit[3] = {3.0, 0.0, 4.0};
  CHECK(normalize3(unit));
  CHECK(unit[0] == Approx(0.6).margin(1e-12));
  CHECK(unit[2] == Approx(0.8).margin(1e-12));
}
