/**
 * @file sensing.cpp
 * @brief Implementation of the sensor, camera and picking math (W7/W8).
 */
#include <srsp/core/sensing.hpp>

#include <algorithm>
#include <cmath>

#include <limits>

namespace srsp::core {

namespace {

constexpr double kPi = 3.14159265358979323846;

Vec3 sub(const Vec3& a, const Vec3& b) {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
Vec3 add(const Vec3& a, const Vec3& b) {
  return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}
Vec3 mul(const Vec3& a, double s) {
  return {a[0] * s, a[1] * s, a[2] * s};
}
double dot(const Vec3& a, const Vec3& b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 normalized(const Vec3& v) {
  const double length = std::sqrt(dot(v, v));
  return length > 0.0 ? mul(v, 1.0 / length) : Vec3{1.0, 0.0, 0.0};
}

std::array<double, 9> rotation_from_quaternion(const std::array<double, 4>& q) {
  const double x = q[0];
  const double y = q[1];
  const double z = q[2];
  const double w = q[3];
  return {1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - z * w),       2.0 * (x * z + y * w),
          2.0 * (x * y + z * w),       1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - x * w),
          2.0 * (x * z - y * w),       2.0 * (y * z + x * w),       1.0 - 2.0 * (x * x + y * y)};
}

Vec3 rotate(const std::array<double, 9>& m, const Vec3& v) {
  return {m[0] * v[0] + m[1] * v[1] + m[2] * v[2], m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
          m[6] * v[0] + m[7] * v[1] + m[8] * v[2]};
}

/** Nearest positive ray/sphere intersection distance, or infinity. */
double ray_sphere(const Vec3& origin, const Vec3& direction, const Vec3& center, double radius) {
  const Vec3 oc = sub(origin, center);
  const double b = dot(oc, direction);
  const double c = dot(oc, oc) - radius * radius;
  const double discriminant = b * b - c;
  if (discriminant < 0.0) {
    return std::numeric_limits<double>::infinity();
  }
  const double root = std::sqrt(discriminant);
  const double near = -b - root;
  if (near > 1e-9) {
    return near;
  }
  const double far = -b + root;
  return far > 1e-9 ? far : std::numeric_limits<double>::infinity();
}

void basis_from_pose(const CameraPose& pose, Vec3* forward, Vec3* right, Vec3* up) {
  *forward = normalized(sub(pose.target, pose.eye));
  Vec3 up_hint = normalized(pose.up);
  if (std::abs(dot(up_hint, *forward)) > 0.999) {
    up_hint = {0.0, 0.0, 1.0};
  }
  *right = normalized(cross(*forward, up_hint));
  *up = normalized(cross(*right, *forward));
}

}  // namespace

CameraPose camera_pose_from_entity(const WorldEntity& entity, double distance,
                                   const Vec3& offset_body) {
  CameraPose pose;
  const Vec3 position{entity.position_m[0], entity.position_m[1], entity.position_m[2]};
  const auto rotation = rotation_from_quaternion(entity.attitude);
  const Vec3 local{offset_body[0], offset_body[1], offset_body[2] + distance};
  const Vec3 offset = rotate(rotation, local);
  pose.eye = add(position, offset);
  pose.target = position;
  pose.up = rotate(rotation, {0.0, 1.0, 0.0});
  pose.valid = true;
  return pose;
}

CameraPose orbit_camera_pose(const OrbitCamera& camera) {
  CameraPose pose;
  const double yaw = camera.yaw_deg * kPi / 180.0;
  const double pitch = camera.pitch_deg * kPi / 180.0;
  const double cp = std::cos(pitch);
  pose.eye = {camera.target[0] + camera.distance * cp * std::sin(yaw),
              camera.target[1] + camera.distance * std::sin(pitch),
              camera.target[2] + camera.distance * cp * std::cos(yaw)};
  pose.target = camera.target;
  pose.up = {0.0, 1.0, 0.0};
  pose.valid = true;
  return pose;
}

std::vector<LidarPoint> lidar_scan(const LidarConfig& config, const CameraPose& pose,
                                   const std::vector<WorldEntity>& targets) {
  std::vector<LidarPoint> points;
  if (!pose.valid || config.rings <= 0 || config.azimuth_samples <= 0) {
    return points;
  }
  Vec3 forward{};
  Vec3 right{};
  Vec3 up{};
  basis_from_pose(pose, &forward, &right, &up);
  const double half_vertical = config.vertical_fov_deg * 0.5 * kPi / 180.0;

  for (int ring = 0; ring < config.rings; ++ring) {
    const double t = config.rings == 1
                         ? 0.0
                         : -half_vertical + 2.0 * half_vertical * static_cast<double>(ring) /
                                                static_cast<double>(config.rings - 1);
    for (int azimuth = 0; azimuth < config.azimuth_samples; ++azimuth) {
      const double yaw =
          2.0 * kPi * static_cast<double>(azimuth) / static_cast<double>(config.azimuth_samples);
      const Vec3 direction = normalized(add(
          add(mul(forward, std::cos(t) * std::cos(yaw)), mul(right, std::cos(t) * std::sin(yaw))),
          mul(up, std::sin(t))));
      double nearest = std::numeric_limits<double>::infinity();
      for (const auto& target : targets) {
        const Vec3 center{target.position_m[0], target.position_m[1], target.position_m[2]};
        const double radius = std::max(1e-6, std::sqrt(target.area_m2 / kPi));
        nearest = std::min(nearest, ray_sphere(pose.eye, direction, center, radius));
      }
      if (nearest <= config.max_range_m) {
        const Vec3 hit = add(pose.eye, mul(direction, nearest));
        LidarPoint point;
        point.x = static_cast<float>(hit[0]);
        point.y = static_cast<float>(hit[1]);
        point.z = static_cast<float>(hit[2]);
        point.intensity = static_cast<float>(1.0 / (1.0 + nearest));
        points.push_back(point);
      }
    }
  }
  return points;
}

std::vector<RadarDetection> radar_scan(const RadarConfig& config, const WorldEntity& observer,
                                       const std::vector<WorldEntity>& targets) {
  std::vector<RadarDetection> detections;
  const Vec3 origin{observer.position_m[0], observer.position_m[1], observer.position_m[2]};
  for (const auto& target : targets) {
    if (target.id == observer.id) {
      continue;
    }
    const Vec3 center{target.position_m[0], target.position_m[1], target.position_m[2]};
    const Vec3 delta = sub(center, origin);
    const double range = std::sqrt(dot(delta, delta));
    if (range <= 0.0 || range > config.max_range_m) {
      continue;
    }
    const Vec3 line_of_sight = mul(delta, 1.0 / range);
    const Vec3 relative{target.velocity_m_s[0] - observer.velocity_m_s[0],
                        target.velocity_m_s[1] - observer.velocity_m_s[1],
                        target.velocity_m_s[2] - observer.velocity_m_s[2]};
    RadarDetection detection;
    detection.range_m = range;
    detection.radial_velocity_m_s = dot(relative, line_of_sight);
    detection.rcs_m2 = target.area_m2;
    detections.push_back(detection);
  }
  return detections;
}

std::optional<std::string> pick_entity(const CameraPose& pose, const CameraIntrinsics& intrinsics,
                                       double ndc_x, double ndc_y,
                                       const std::vector<WorldEntity>& targets) {
  if (!pose.valid) {
    return std::nullopt;
  }
  Vec3 forward{};
  Vec3 right{};
  Vec3 up{};
  basis_from_pose(pose, &forward, &right, &up);
  const double half_height = std::tan(intrinsics.fov_deg * 0.5 * kPi / 180.0);
  const double half_width = half_height * intrinsics.aspect;
  const Vec3 direction =
      normalized(add(add(forward, mul(right, ndc_x * half_width)), mul(up, ndc_y * half_height)));

  double nearest = std::numeric_limits<double>::infinity();
  std::optional<std::string> hit;
  for (const auto& target : targets) {
    const Vec3 center{target.position_m[0], target.position_m[1], target.position_m[2]};
    const double radius = std::max(1e-6, std::sqrt(target.area_m2 / kPi));
    const double distance = ray_sphere(pose.eye, direction, center, radius);
    if (distance < nearest) {
      nearest = distance;
      hit = target.id;
    }
  }
  return hit;
}

SensorScheduler::SensorScheduler(std::vector<SensorMount> sensors) {
  for (auto& sensor : sensors) {
    if (sensor.rate_hz <= 0.0 || sensor.entity_id.empty()) {
      continue;
    }
    Entry entry;
    entry.mount = std::move(sensor);
    entry.next_s = entry.mount.phase_s + 1.0 / entry.mount.rate_hz;
    entries_.push_back(std::move(entry));
  }
}

std::vector<SensorSample> SensorScheduler::advance(double dt) {
  std::vector<SensorSample> samples;
  if (!(dt > 0.0)) {
    return samples;
  }
  time_s_ += dt;
  for (auto& entry : entries_) {
    const double period = 1.0 / entry.mount.rate_hz;
    while (entry.next_s <= time_s_ + 1e-12) {
      samples.push_back(SensorSample{entry.mount.entity_id, entry.mount.kind, entry.next_s});
      entry.next_s += period;
    }
  }
  return samples;
}
}  // namespace srsp::core
