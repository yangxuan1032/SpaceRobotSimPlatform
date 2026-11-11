/**
 * @file studio_camera.cpp
 * @brief W8: orbit camera and picking for the studio window.
 */
#include <srsp/core/sensing.hpp>

#include <cmath>
#include <vector>

#include <QMouseEvent>
#include <QStatusBar>
#include <QWheelEvent>

#include "studio_window.hpp"

namespace srsp::studio {

namespace {

srsp::render::CameraView to_view(const srsp::core::CameraPose& pose, double fov_deg) {
  srsp::render::CameraView view{};
  for (int axis = 0; axis < 3; ++axis) {
    view.eye[axis] = pose.eye[static_cast<std::size_t>(axis)];
    view.target[axis] = pose.target[static_cast<std::size_t>(axis)];
    view.up[axis] = pose.up[static_cast<std::size_t>(axis)];
  }
  view.fov_deg = fov_deg;
  return view;
}

}  // namespace

void StudioWindow::set_orbit_camera(double yaw_deg, double pitch_deg, double distance) {
  yaw_deg_ = yaw_deg;
  pitch_deg_ = std::clamp(pitch_deg, -89.0, 89.0);
  distance_m_ = std::max(0.5, distance);
  apply_camera();
}

void StudioWindow::apply_camera() {
  srsp::core::OrbitCamera orbit;
  orbit.yaw_deg = yaw_deg_;
  orbit.pitch_deg = pitch_deg_;
  orbit.distance = distance_m_;
  const auto pose = srsp::core::orbit_camera_pose(orbit);
  const auto view = to_view(pose, 45.0);
  if (renderer_ != nullptr) {
    renderer_->set_camera(view);
  }
  if (rhi_ != nullptr) {
    rhi_->set_camera(view);
  }
  update();
}

QString StudioWindow::pick_at(double ndc_x, double ndc_y) const {
  srsp::core::OrbitCamera orbit;
  orbit.yaw_deg = yaw_deg_;
  orbit.pitch_deg = pitch_deg_;
  orbit.distance = distance_m_;
  const auto pose = srsp::core::orbit_camera_pose(orbit);

  std::vector<srsp::core::WorldEntity> entities;
  for (std::size_t i = 0; i < scene_.bodies.size(); ++i) {
    srsp::core::WorldEntity entity;
    entity.id = "body/" + std::to_string(i);
    for (int axis = 0; axis < 3; ++axis) {
      entity.position_m[static_cast<std::size_t>(axis)] =
          scene_.bodies[i].position_m[static_cast<std::size_t>(axis)];
    }
    entity.area_m2 = 3.14159265358979323846 * scene_.bodies[i].radius_m * scene_.bodies[i].radius_m;
    entities.push_back(entity);
  }
  srsp::core::CameraIntrinsics intrinsics;
  intrinsics.fov_deg = 45.0;
  intrinsics.aspect = 16.0 / 9.0;
  const auto hit = srsp::core::pick_entity(pose, intrinsics, ndc_x, ndc_y, entities);
  return hit.has_value() ? QString::fromStdString(*hit) : QString();
}

void StudioWindow::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    dragging_ = true;
    last_mouse_ = event->position().toPoint();
  }
  QMainWindow::mousePressEvent(event);
}

void StudioWindow::mouseMoveEvent(QMouseEvent* event) {
  if (!dragging_) {
    QMainWindow::mouseMoveEvent(event);
    return;
  }
  const QPoint delta = event->position().toPoint() - last_mouse_;
  last_mouse_ = event->position().toPoint();
  yaw_deg_ -= delta.x() * 0.5;
  pitch_deg_ = std::clamp(pitch_deg_ + delta.y() * 0.5, -89.0, 89.0);
  apply_camera();
  QMainWindow::mouseMoveEvent(event);
}

void StudioWindow::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && dragging_) {
    dragging_ = false;
    const auto size = viewport_ != nullptr ? viewport_->size() : this->size();
    const double width = std::max(1, size.width());
    const double height = std::max(1, size.height());
    const double ndc_x = 2.0 * event->position().x() / width - 1.0;
    const double ndc_y = 1.0 - 2.0 * event->position().y() / height;
    picked_ = pick_at(ndc_x, ndc_y);
    if (statusBar() != nullptr) {
      statusBar()->showMessage(picked_.isEmpty() ? QStringLiteral("picked: none")
                                                 : QStringLiteral("picked: ") + picked_);
    }
  }
  QMainWindow::mouseReleaseEvent(event);
}

void StudioWindow::wheelEvent(QWheelEvent* event) {
  const double steps = event->angleDelta().y() / 120.0;
  distance_m_ = std::max(0.5, distance_m_ * std::pow(0.9, steps));
  apply_camera();
  event->accept();
}

}  // namespace srsp::studio