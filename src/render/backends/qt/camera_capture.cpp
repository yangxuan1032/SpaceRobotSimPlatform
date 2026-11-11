/**
 * @file camera_capture.cpp
 * @brief W7: camera sensor capture through the Qt renderer.
 */
#include <srsp/render/camera_capture.hpp>

#include "qt_renderer.hpp"

namespace srsp::render {

CameraFrame capture_camera(const CameraView& camera, const Scene& scene, int width, int height) {
  CameraFrame frame;
  if (width <= 0 || height <= 0) {
    return frame;
  }
  srsp::studio::QtRenderer renderer;
  std::string error;
  const Viewport viewport{width, height};
  if (!renderer.initialize(nullptr, viewport, &error)) {
    return frame;
  }
  renderer.set_camera(camera);
  renderer.set_scene(scene);
  for (int step = 0; step < 2; ++step) {
    if (!renderer.render_frame()) {
      renderer.shutdown();
      return frame;
    }
  }
  const QImage image =
      renderer.render_image(width, height).convertToFormat(QImage::Format_RGBA8888);
  renderer.shutdown();
  if (image.isNull()) {
    return frame;
  }
  frame.width = image.width();
  frame.height = image.height();
  frame.rgba.resize(static_cast<std::size_t>(frame.width) * frame.height * 4u);
  for (int y = 0; y < frame.height; ++y) {
    std::memcpy(frame.rgba.data() + static_cast<std::size_t>(y) * frame.width * 4u,
                image.constScanLine(y), static_cast<std::size_t>(frame.width) * 4u);
  }
  return frame;
}

}  // namespace srsp::render