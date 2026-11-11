/**
 * @file test_camera_capture.cpp
 * @brief W7: a camera sensor frame renders the scene through the same pose.
 */
#include <srsp/render/camera_capture.hpp>
#include <srsp/render/scene.hpp>

#include <catch2/catch_test_macros.hpp>

using srsp::render::CameraFrame;
using srsp::render::CameraView;
using srsp::render::capture_camera;
using srsp::render::Scene;
using srsp::render::Sphere;

namespace {

struct Rgba {
  int red = 0;
  int green = 0;
  int blue = 0;
};

Rgba pixel_at(const CameraFrame& frame, int x, int y) {
  const std::size_t index = (static_cast<std::size_t>(y) * frame.width + x) * 4u;
  return Rgba{frame.rgba[index], frame.rgba[index + 1], frame.rgba[index + 2]};
}

}  // namespace

TEST_CASE("a camera capture renders the sphere in front of it", "[camera_capture]") {
  Scene scene{};
  Sphere body{};
  body.position_m[2] = 10.0;
  body.radius_m = 1.0;
  body.color[0] = 0.9f;
  body.color[1] = 0.2f;
  body.color[2] = 0.2f;
  body.orientation[3] = 1.0;
  scene.bodies.push_back(body);

  CameraView camera{};
  camera.eye[2] = 0.0;
  camera.target[2] = 1.0;
  camera.up[1] = 1.0;
  camera.fov_deg = 45.0;
  camera.far_m = 1.0e9;

  const CameraFrame frame = capture_camera(camera, scene, 96, 64);
  REQUIRE(frame.valid());
  CHECK(frame.width == 96);
  CHECK(frame.height == 64);

  const Rgba centre = pixel_at(frame, 48, 32);
  CHECK(centre.red > 100);
  CHECK(centre.red > centre.blue);

  const Rgba corner = pixel_at(frame, 1, 1);
  CHECK(corner.red < 100);
}

TEST_CASE("an invalid capture size yields an invalid frame", "[camera_capture]") {
  CameraView camera{};
  Scene scene{};
  CHECK_FALSE(capture_camera(camera, scene, 0, 64).valid());
  CHECK_FALSE(capture_camera(camera, scene, 64, -1).valid());
}