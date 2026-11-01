/**
 * @file qt_renderer.hpp
 * @brief Software renderer that draws a Scene with QPainter.
 * @brief_zh 使用 QPainter 绘制场景的软件渲染器。
 *
 * This is the M1 visual backend: it proves the renderer contract, the
 * dual-origin projection and the scene hand-off without requiring a GPU
 * backend. The frame can be painted into a widget or into an offscreen QImage
 * (used by the tests), so the same code path is exercised headlessly.
 */
#ifndef SRSP_STUDIO_QT_RENDERER_HPP
#define SRSP_STUDIO_QT_RENDERER_HPP

#include <srsp/render/renderer.hpp>
#include <srsp/render/scene.hpp>

#include <cstdint>
#include <string>

#include <QImage>
#include <QPainter>

namespace srsp::studio {

/** @brief QPainter-based renderer for orbital scenes. */
/** @brief_zh 基于 QPainter 的轨道场景渲染器。 */
class QtRenderer final : public srsp::render::Renderer {
 public:
  [[nodiscard]] const char* name() const noexcept override { return "qt"; }

  bool initialize(void* native_window, const srsp::render::Viewport& viewport,
                  std::string* error) override;
  bool resize(const srsp::render::Viewport& viewport) override;
  void set_camera(const srsp::render::CameraView& camera) override;
  bool render_frame() override;
  [[nodiscard]] std::uint64_t frame_count() const noexcept override { return frames_; }
  void shutdown() override;

  /** @brief Replaces the scene drawn by the next frame. */
  /** @brief_zh 替换下一帧绘制的场景。 */
  void set_scene(const srsp::render::Scene& scene) { scene_ = scene; }
  [[nodiscard]] const srsp::render::Scene& scene() const noexcept { return scene_; }
  [[nodiscard]] const srsp::render::CameraView& camera() const noexcept { return camera_; }

  /** @brief Paints the current scene into an existing painter. */
  /** @brief_zh 将当前场景绘制到已有的 painter。 */
  void paint(QPainter& painter, const srsp::render::Viewport& viewport) const;

  /** @brief Renders the current scene into a new image (offscreen/tests). */
  /** @brief_zh 将当前场景渲染为新图像（离屏/测试用）。 */
  [[nodiscard]] QImage render_image(int width, int height) const;

 private:
  srsp::render::Viewport viewport_{};
  srsp::render::CameraView camera_{};
  srsp::render::Scene scene_{};
  std::uint64_t frames_ = 0;
  bool initialized_ = false;
  void* native_window_ = nullptr;
};

}  // namespace srsp::studio

#endif  // SRSP_STUDIO_QT_RENDERER_HPP
