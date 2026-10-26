/**
 * @file qt_renderer.cpp
 * @brief QPainter implementation of the software renderer.
 */
#include "qt_renderer.hpp"

#include <srsp/render/projection.hpp>

#include <cmath>
#include <vector>

#include <QBrush>
#include <QColor>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPen>

namespace srsp::studio {
namespace {

QColor to_color(const float rgb[3], int alpha = 255) {
  auto channel = [](float value) {
    const int scaled = static_cast<int>(std::lround(value * 255.0f));
    return scaled < 0 ? 0 : (scaled > 255 ? 255 : scaled);
  };
  return QColor(channel(rgb[0]), channel(rgb[1]), channel(rgb[2]), alpha);
}

/// Deterministic star field: a hash keeps frames identical between runs.
void draw_stars(QPainter& painter, const srsp::render::Viewport& viewport, std::uint32_t seed) {
  painter.save();
  const int count = 220;
  for (int i = 0; i < count; ++i) {
    std::uint32_t hash = static_cast<std::uint32_t>(i) * 2654435761u + seed;
    hash ^= hash >> 13;
    hash *= 1274126177u;
    const double x = static_cast<double>(hash % 10000u) / 10000.0 * viewport.width;
    const double y = static_cast<double>((hash >> 8) % 10000u) / 10000.0 * viewport.height;
    const int brightness = 90 + static_cast<int>((hash >> 16) % 160u);
    painter.setPen(QPen(QColor(brightness, brightness, brightness)));
    painter.drawPoint(QPointF(x, y));
  }
  painter.restore();
}

}  // namespace

bool QtRenderer::initialize(void* native_window, const srsp::render::Viewport& viewport,
                            std::string* error) {
  if (initialized_) {
    if (error != nullptr) {
      *error = "renderer already initialised";
    }
    return false;
  }
  if (viewport.width <= 0 || viewport.height <= 0) {
    if (error != nullptr) {
      *error = "viewport must be positive";
    }
    return false;
  }
  native_window_ = native_window;
  viewport_ = viewport;
  initialized_ = true;
  frames_ = 0;
  return true;
}

bool QtRenderer::resize(const srsp::render::Viewport& viewport) {
  if (!initialized_ || viewport.width <= 0 || viewport.height <= 0) {
    return false;
  }
  viewport_ = viewport;
  return true;
}

void QtRenderer::set_camera(const srsp::render::CameraView& camera) {
  camera_ = camera;
}

bool QtRenderer::render_frame() {
  if (!initialized_) {
    return false;
  }
  ++frames_;
  return true;
}

void QtRenderer::shutdown() {
  initialized_ = false;
  native_window_ = nullptr;
  frames_ = 0;
}

void QtRenderer::paint(QPainter& painter, const srsp::render::Viewport& viewport) const {
  painter.setRenderHint(QPainter::Antialiasing, true);

  QLinearGradient background(0.0, 0.0, 0.0, static_cast<double>(viewport.height));
  background.setColorAt(0.0, QColor(8, 10, 16));
  background.setColorAt(1.0, QColor(18, 22, 32));
  painter.fillRect(0, 0, viewport.width, viewport.height, background);

  draw_stars(painter, viewport, 0x5eedu);

  // Planet: projected with its own origin to keep the precision at 1e7 m.
  if (scene_.has_earth) {
    const srsp::render::ScreenPoint centre =
        srsp::render::project(camera_, viewport, scene_.earth.position_m, scene_.earth.position_m);
    if (centre.visible) {
      const double radius_px = scene_.earth.radius_m * centre.scale;
      if (radius_px > 0.5) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(to_color(scene_.earth.color)));
        painter.drawEllipse(QPointF(centre.x, centre.y), radius_px, radius_px);

        // Day/night terminator: shade the half-plane facing away from the Sun.
        const double sun_screen_x = scene_.sun_direction[0];
        const double sun_screen_y = -scene_.sun_direction[2];
        QPainterPath shadow;
        shadow.addEllipse(QPointF(centre.x, centre.y), radius_px, radius_px);
        QPainterPath night;
        const double normal = std::hypot(sun_screen_x, sun_screen_y);
        if (normal > 1e-6) {
          const double nx = sun_screen_x / normal;
          const double ny = sun_screen_y / normal;
          night.moveTo(centre.x - ny * radius_px * 1.5 - nx * radius_px * 0.2,
                       centre.y + nx * radius_px * 1.5 - ny * radius_px * 0.2);
          night.lineTo(centre.x + ny * radius_px * 1.5 - nx * radius_px * 0.2,
                       centre.y - nx * radius_px * 1.5 - ny * radius_px * 0.2);
          night.lineTo(centre.x + ny * radius_px * 1.5 + nx * radius_px * 1.5,
                       centre.y - nx * radius_px * 1.5 + ny * radius_px * 1.5);
          night.lineTo(centre.x - ny * radius_px * 1.5 + nx * radius_px * 1.5,
                       centre.y + nx * radius_px * 1.5 + ny * radius_px * 1.5);
          night.closeSubpath();
          painter.setBrush(QBrush(QColor(10, 12, 20, 170)));
          painter.setClipPath(shadow);
          painter.drawPath(night);
          painter.setClipping(false);
        }
      }
    }
  }

  // Robots: projected relative to the camera position (dual origin again).
  for (const auto& body : scene_.bodies) {
    const srsp::render::ScreenPoint projected =
        srsp::render::project(camera_, viewport, body.position_m, camera_.eye);
    if (!projected.visible) {
      continue;
    }
    const double radius_px = std::max(1.5, body.radius_m * projected.scale);
    painter.setPen(QPen(QColor(230, 230, 235), 1.0));
    painter.setBrush(QBrush(to_color(body.color)));
    painter.drawEllipse(QPointF(projected.x, projected.y), radius_px, radius_px);
  }
}

QImage QtRenderer::render_image(int width, int height) const {
  QImage image(width, height, QImage::Format_RGB32);
  image.fill(QColor(8, 10, 16));
  QPainter painter(&image);
  paint(painter, srsp::render::Viewport{width, height});
  painter.end();
  return image;
}

}  // namespace srsp::studio
