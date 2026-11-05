/**
 * @file qt_renderer.cpp
 * @brief QPainter implementation of the software renderer.
 */
#include "qt_renderer.hpp"

#include <srsp/render/mesh_loader.hpp>
#include <srsp/render/mesh_source.hpp>
#include <srsp/render/projection.hpp>

#include <algorithm>
#include <array>
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

/// Rotation matrix (row-major) from a body->world quaternion (x, y, z, w).
std::array<double, 9> quat_to_matrix(const double q[4]) {
  const double x = q[0];
  const double y = q[1];
  const double z = q[2];
  const double w = q[3];
  const double norm = std::sqrt(x * x + y * y + z * z + w * w);
  const double s = norm > 0.0 ? 1.0 / norm : 1.0;
  const double xx = x * s * x * s;
  const double yy = y * s * y * s;
  const double zz = z * s * z * s;
  const double xy = x * s * y * s;
  const double xz = x * s * z * s;
  const double yz = y * s * z * s;
  const double wx = w * s * x * s;
  const double wy = w * s * y * s;
  const double wz = w * s * z * s;
  return {1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz),       2.0 * (xz + wy),
          2.0 * (xy + wz),       1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx),
          2.0 * (xz - wy),       2.0 * (yz + wx),       1.0 - 2.0 * (xx + yy)};
}

void rotate(const std::array<double, 9>& m, const double v[3], double out[3]) {
  out[0] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2];
  out[1] = m[3] * v[0] + m[4] * v[1] + m[5] * v[2];
  out[2] = m[6] * v[0] + m[7] * v[1] + m[8] * v[2];
}

double dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/// Draws a loaded triangle mesh with flat shading, far triangles first.
void draw_mesh(QPainter& painter, const srsp::render::CameraView& camera,
               const srsp::render::Viewport& viewport, const srsp::render::Sphere& body,
               const srsp::render::MeshData& mesh) {
  const std::array<double, 9> rotation = quat_to_matrix(body.orientation);
  const double light[3] = {0.35, 0.45, 0.82};

  struct Triangle {
    srsp::render::ScreenPoint points[3];
    double depth = 0.0;
    double shade = 1.0;
  };
  std::vector<Triangle> triangles;
  triangles.reserve(mesh.indices.size() / 3u);

  for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    Triangle triangle;
    double world[3][3];
    bool visible = true;
    for (int corner = 0; corner < 3; ++corner) {
      const std::size_t index = mesh.indices[i + static_cast<std::size_t>(corner)];
      const double local[3] = {mesh.positions[index * 3u], mesh.positions[index * 3u + 1u],
                               mesh.positions[index * 3u + 2u]};
      double rotated[3];
      rotate(rotation, local, rotated);
      for (int axis = 0; axis < 3; ++axis) {
        world[corner][axis] = body.position_m[axis] + rotated[axis];
      }
      triangle.points[corner] = srsp::render::project(camera, viewport, world[corner], camera.eye);
      visible = visible && triangle.points[corner].visible;
    }
    if (!visible) {
      continue;
    }

    const double e1[3] = {world[1][0] - world[0][0], world[1][1] - world[0][1],
                          world[1][2] - world[0][2]};
    const double e2[3] = {world[2][0] - world[0][0], world[2][1] - world[0][1],
                          world[2][2] - world[0][2]};
    const double normal[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                              e1[0] * e2[1] - e1[1] * e2[0]};
    const double length =
        std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    const double dot =
        length > 1e-12
            ? (normal[0] * light[0] + normal[1] * light[1] + normal[2] * light[2]) / length
            : 0.0;
    triangle.shade = 0.35 + 0.65 * std::max(0.0, dot);

    const double cx = (world[0][0] + world[1][0] + world[2][0]) / 3.0;
    const double cy = (world[0][1] + world[1][1] + world[2][1]) / 3.0;
    const double cz = (world[0][2] + world[1][2] + world[2][2]) / 3.0;
    triangle.depth = std::sqrt((cx - camera.eye[0]) * (cx - camera.eye[0]) +
                               (cy - camera.eye[1]) * (cy - camera.eye[1]) +
                               (cz - camera.eye[2]) * (cz - camera.eye[2]));
    triangles.push_back(triangle);
  }

  std::sort(triangles.begin(), triangles.end(),
            [](const Triangle& a, const Triangle& b) { return a.depth > b.depth; });

  painter.setPen(Qt::NoPen);
  for (const Triangle& triangle : triangles) {
    const QColor base = to_color(body.color);
    const QColor shaded(static_cast<int>(base.red() * triangle.shade),
                        static_cast<int>(base.green() * triangle.shade),
                        static_cast<int>(base.blue() * triangle.shade));
    painter.setBrush(QBrush(shaded));
    const QPolygonF polygon({QPointF(triangle.points[0].x, triangle.points[0].y),
                             QPointF(triangle.points[1].x, triangle.points[1].y),
                             QPointF(triangle.points[2].x, triangle.points[2].y)});
    painter.drawPolygon(polygon);
  }
}
/// Draws an oriented box mesh as six depth-sorted, shaded quads.
void draw_box(QPainter& painter, const srsp::render::CameraView& camera,
              const srsp::render::Viewport& viewport, const srsp::render::Sphere& body) {
  const std::array<double, 9> rotation = quat_to_matrix(body.orientation);
  const double half = body.radius_m;
  const double signs[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
                              {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};

  std::array<srsp::render::ScreenPoint, 8> projected{};
  for (int i = 0; i < 8; ++i) {
    const double local[3] = {signs[i][0] * half, signs[i][1] * half, signs[i][2] * half};
    double rotated[3];
    rotate(rotation, local, rotated);
    const double world[3] = {body.position_m[0] + rotated[0], body.position_m[1] + rotated[1],
                             body.position_m[2] + rotated[2]};
    projected[static_cast<std::size_t>(i)] =
        srsp::render::project(camera, viewport, world, camera.eye);
    if (!projected[static_cast<std::size_t>(i)].visible) {
      return;
    }
  }

  const int faces[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
                           {2, 3, 7, 6}, {1, 2, 6, 5}, {0, 3, 7, 4}};
  const double normals[6][3] = {{0, 0, -1}, {0, 0, 1}, {0, -1, 0},
                                {0, 1, 0},  {1, 0, 0}, {-1, 0, 0}};
  const double light[3] = {0.4, 0.6, 0.7};

  struct Face {
    double depth;
    int index;
  };
  std::array<Face, 6> order{};
  for (int f = 0; f < 6; ++f) {
    double depth = 0.0;
    for (int c = 0; c < 4; ++c) {
      depth += projected[static_cast<std::size_t>(faces[f][c])].depth;
    }
    order[static_cast<std::size_t>(f)] = {depth, f};
  }
  std::sort(order.begin(), order.end(),
            [](const Face& a, const Face& b) { return a.depth > b.depth; });

  for (const Face& face : order) {
    const int f = face.index;
    double normal_world[3];
    rotate(rotation, normals[static_cast<std::size_t>(f)], normal_world);
    const double shade = 0.35 + 0.65 * std::max(0.0, dot3(normal_world, light));
    QColor color = to_color(body.color);
    color = QColor(static_cast<int>(color.red() * shade), static_cast<int>(color.green() * shade),
                   static_cast<int>(color.blue() * shade));
    QPolygonF polygon;
    for (int c = 0; c < 4; ++c) {
      const auto& point = projected[static_cast<std::size_t>(faces[f][c])];
      polygon << QPointF(point.x, point.y);
    }
    painter.setPen(QPen(color.darker(140), 1.0));
    painter.setBrush(QBrush(color));
    painter.drawPolygon(polygon);
  }
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
    const srsp::render::MeshSource source = srsp::render::mesh_source_from_uri(body.mesh_uri);
    if (source == srsp::render::MeshSource::kBox) {
      draw_box(painter, camera_, viewport, body);
      continue;
    }
    if (source == srsp::render::MeshSource::kGltf) {
      auto& cached = mesh_cache_[body.mesh_uri];
      if (cached.empty()) {
        const srsp::render::MeshLoadResult loaded = srsp::render::load_glb(body.mesh_uri);
        if (loaded.ok()) {
          cached = loaded.mesh;
        } else {
          static bool warned = false;
          if (!warned) {
            std::fprintf(stderr, "warn: %s; drawing a sphere instead\n", loaded.error.c_str());
            warned = true;
          }
        }
      }
      if (!cached.empty()) {
        draw_mesh(painter, camera_, viewport, body, cached);
        continue;
      }
    }
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
