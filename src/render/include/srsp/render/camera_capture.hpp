/**
 * @file camera_capture.hpp
 * @brief W7: renders a camera sensor frame from a CameraView and a Scene.
 *
 * A camera sensor is the renderer seen through a pose: the studio and the sensor
 * share CameraView, so a capture can never disagree with the viewport. The frame
 * is plain RGBA8 bytes so the header stays free of any Qt type.
 */
#ifndef SRSP_RENDER_CAMERA_CAPTURE_HPP
#define SRSP_RENDER_CAMERA_CAPTURE_HPP

#include <srsp/render/renderer.hpp>
#include <srsp/render/scene.hpp>

#include <cstdint>
#include <vector>

namespace srsp::render {

/** @brief One captured camera frame in RGBA8, row-major. */
struct CameraFrame {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba;

  [[nodiscard]] bool valid() const noexcept {
    return width > 0 && height > 0 && rgba.size() == static_cast<std::size_t>(width) * height * 4u;
  }
};

/**
 * @brief Renders the scene from @p camera into an RGBA8 frame.
 * @param camera View transform; fov/near/far are honoured.
 * @param scene Scene to draw.
 * @param width Output width in pixels.
 * @param height Output height in pixels.
 * @return The frame; invalid when the arguments are out of range.
 */
[[nodiscard]] CameraFrame capture_camera(const CameraView& camera, const Scene& scene, int width,
                                         int height);

}  // namespace srsp::render

#endif  // SRSP_RENDER_CAMERA_CAPTURE_HPP