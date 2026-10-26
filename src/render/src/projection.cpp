/**
 * @file projection.cpp
 * @brief Pinhole projection with an explicit origin.
 */
#include <srsp/render/projection.hpp>

#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
}

namespace srsp::render {
namespace {

double length3(const double v[3]) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

void sub3(const double a[3], const double b[3], double out[3]) {
  out[0] = a[0] - b[0];
  out[1] = a[1] - b[1];
  out[2] = a[2] - b[2];
}

}  // namespace

double dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void cross3(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

bool normalize3(double vector[3]) {
  const double norm = length3(vector);
  if (norm <= 0.0) {
    return false;
  }
  vector[0] /= norm;
  vector[1] /= norm;
  vector[2] /= norm;
  return true;
}

ScreenPoint project(const CameraView& camera, const Viewport& viewport, const double point[3],
                    const double origin[3]) {
  ScreenPoint result;
  if (viewport.width <= 0 || viewport.height <= 0) {
    return result;
  }

  // Work relative to the explicit origin: this is the dual-origin trick.
  double eye[3];
  double target[3];
  sub3(camera.eye, origin, eye);
  sub3(camera.target, origin, target);

  double forward[3];
  sub3(target, eye, forward);
  if (!normalize3(forward)) {
    return result;
  }
  double right[3];
  cross3(forward, camera.up, right);
  if (!normalize3(right)) {
    return result;
  }
  double up[3];
  cross3(right, forward, up);

  // Point and eye are both expressed relative to the explicit origin, then the
  // camera-relative vector is formed; this keeps cancellation bounded at 1e7 m.
  double point_relative[3];
  sub3(point, origin, point_relative);
  double offset[3];
  sub3(point_relative, eye, offset);
  const double z = dot3(offset, forward);
  if (z <= camera.near_m) {
    return result;
  }

  const double focal =
      (0.5 * static_cast<double>(viewport.height)) / std::tan(0.5 * camera.fov_deg * kPi / 180.0);
  result.x = 0.5 * static_cast<double>(viewport.width) + dot3(offset, right) * focal / z;
  result.y = 0.5 * static_cast<double>(viewport.height) - dot3(offset, up) * focal / z;
  result.depth = z;
  result.scale = focal / z;
  result.visible = true;
  return result;
}

}  // namespace srsp::render
