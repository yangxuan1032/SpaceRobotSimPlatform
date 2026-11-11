/**
 * @file test_sensing.cpp
 * @brief W7/W8: camera poses, lidar sweep, radar detections and picking.
 */
#include <srsp/core/sensing.hpp>

#include <cmath>

#include <catch2/catch_test_macros.hpp>

using srsp::core::camera_pose_from_entity;
using srsp::core::CameraIntrinsics;
using srsp::core::lidar_scan;
using srsp::core::LidarConfig;
using srsp::core::orbit_camera_pose;
using srsp::core::OrbitCamera;
using srsp::core::pick_entity;
using srsp::core::radar_scan;
using srsp::core::RadarConfig;
using srsp::core::SensorMount;
using srsp::core::SensorScheduler;
using srsp::core::WorldEntity;

namespace {

constexpr double kPi = 3.14159265358979323846;

WorldEntity make_sphere(const char* id, double x, double y, double z, double radius) {
  WorldEntity entity;
  entity.id = id;
  entity.position_m = {x, y, z};
  entity.area_m2 = kPi * radius * radius;
  return entity;
}

}  // namespace

TEST_CASE("camera pose sits behind the entity along body +Z", "[sensing]") {
  WorldEntity craft;
  craft.id = "entity/craft";
  craft.position_m = {1.0, 2.0, 3.0};
  craft.attitude = {0.0, 0.0, 0.0, 1.0};
  const auto pose = camera_pose_from_entity(craft, 5.0);
  CHECK(pose.valid);
  CHECK(pose.eye[0] == 1.0);
  CHECK(pose.eye[1] == 2.0);
  CHECK(pose.eye[2] == 8.0);
  CHECK(pose.target[2] == 3.0);
  CHECK(pose.up[1] == 1.0);
}

TEST_CASE("orbit camera maps yaw and pitch to a position", "[sensing]") {
  OrbitCamera camera;
  camera.target = {0.0, 0.0, 0.0};
  camera.distance = 5.0;
  const auto front = orbit_camera_pose(camera);
  CHECK(std::abs(front.eye[2] - 5.0) < 1e-12);

  camera.yaw_deg = 90.0;
  const auto side = orbit_camera_pose(camera);
  CHECK(std::abs(side.eye[0] - 5.0) < 1e-12);

  camera.yaw_deg = 0.0;
  camera.pitch_deg = 90.0;
  const auto top = orbit_camera_pose(camera);
  CHECK(std::abs(top.eye[1] - 5.0) < 1e-12);
}

TEST_CASE("lidar returns the nearest sphere along the ray", "[sensing]") {
  LidarConfig config;
  config.rings = 1;
  config.azimuth_samples = 1;
  config.max_range_m = 100.0;

  srsp::core::CameraPose pose;
  pose.eye = {0.0, 0.0, 0.0};
  pose.target = {0.0, 0.0, 1.0};
  pose.up = {0.0, 1.0, 0.0};
  pose.valid = true;

  const std::vector<WorldEntity> targets{make_sphere("entity/target", 0.0, 0.0, 10.0, 1.0)};
  const auto points = lidar_scan(config, pose, targets);
  REQUIRE(points.size() == 1u);
  CHECK(std::abs(points[0].z - 9.0f) < 1e-4f);
  CHECK(points[0].intensity > 0.0f);

  config.max_range_m = 5.0;
  CHECK(lidar_scan(config, pose, targets).empty());
}

TEST_CASE("radar reports range and radial velocity", "[sensing]") {
  RadarConfig config;
  config.max_range_m = 100.0;
  WorldEntity observer;
  observer.id = "entity/observer";

  WorldEntity target = make_sphere("entity/target", 0.0, 0.0, 10.0, 1.0);
  target.velocity_m_s = {0.0, 0.0, 1.0};
  const auto detections = radar_scan(config, observer, {target});
  REQUIRE(detections.size() == 1u);
  CHECK(std::abs(detections[0].range_m - 10.0) < 1e-12);
  CHECK(std::abs(detections[0].radial_velocity_m_s - 1.0) < 1e-12);

  config.max_range_m = 5.0;
  CHECK(radar_scan(config, observer, {target}).empty());
}

TEST_CASE("picking hits the entity under the cursor", "[sensing]") {
  srsp::core::CameraPose pose;
  pose.eye = {0.0, 0.0, 0.0};
  pose.target = {0.0, 0.0, 1.0};
  pose.up = {0.0, 1.0, 0.0};
  pose.valid = true;

  CameraIntrinsics intrinsics;
  intrinsics.fov_deg = 90.0;
  intrinsics.aspect = 1.0;

  const std::vector<WorldEntity> targets{make_sphere("entity/target", 0.0, 0.0, 10.0, 1.0)};
  const auto centre = pick_entity(pose, intrinsics, 0.0, 0.0, targets);
  REQUIRE(centre.has_value());
  CHECK(*centre == "entity/target");

  const auto off = pick_entity(pose, intrinsics, 0.9, 0.9, targets);
  CHECK_FALSE(off.has_value());
}
TEST_CASE("the sensor schedule follows the configured rates", "[sensing]") {
  std::vector<SensorMount> mounts;
  mounts.push_back(SensorMount{"entity/observer", "camera", 10.0, 0.0});
  mounts.push_back(SensorMount{"entity/observer", "lidar", 2.0, 0.1});
  SensorScheduler scheduler(mounts);

  int camera = 0;
  int lidar = 0;
  for (int i = 0; i < 100; ++i) {
    const auto samples = scheduler.advance(0.01);
    for (const auto& sample : samples) {
      if (sample.kind == "camera") {
        ++camera;
      } else if (sample.kind == "lidar") {
        ++lidar;
      }
    }
  }
  CHECK(camera == 10);
  // 2 Hz with a 0.1 s phase: one sample at 0.6 s inside the first second.
  CHECK(lidar == 1);
  CHECK(std::abs(scheduler.time_s() - 1.0) < 1e-12);
}

TEST_CASE("the sensor schedule is deterministic", "[sensing]") {
  std::vector<SensorMount> mounts{SensorMount{"entity/a", "camera", 4.0, 0.0}};
  const auto run = [&mounts]() {
    SensorScheduler scheduler(mounts);
    std::vector<double> times;
    for (int i = 0; i < 100; ++i) {
      for (const auto& sample : scheduler.advance(0.01)) {
        times.push_back(sample.t_s);
      }
    }
    return times;
  };
  CHECK(run() == run());
}

TEST_CASE("a mounted camera follows the entity attitude", "[sensing]") {
  WorldEntity craft;
  craft.id = "entity/craft";
  // 90 degrees about Z.
  craft.attitude = {0.0, 0.0, 0.70710678, 0.70710678};
  const auto pose = camera_pose_from_entity(craft, 5.0, {1.0, 0.0, 0.0});
  // The body X offset rotates onto +Y and the eye stays 5 m behind.
  CHECK(std::abs(pose.eye[0]) < 1e-6);
  CHECK(std::abs(pose.eye[1] - 1.0) < 1e-6);
  CHECK(std::abs(pose.eye[2] - 5.0) < 1e-6);
}