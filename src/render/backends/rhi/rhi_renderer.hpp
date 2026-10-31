/**
 * @file rhi_renderer.hpp
 * @brief renderer.rhi: GPU renderer built on Qt's RHI abstraction.
 *
 * The backend renders offscreen (no swapchain): a colour texture is cleared
 * each frame and read back on demand. This is enough to validate the RHI
 * pipeline, device creation and the renderer contract without owning a window;
 * the studio will hand it a surface in a later step.
 */
#ifndef SRSP_RENDER_BACKEND_RHI_HPP
#define SRSP_RENDER_BACKEND_RHI_HPP

#include <srsp/render/renderer.hpp>
#include <srsp/render/scene.hpp>

#include <cstdint>
#include <memory>
#include <string>

#include <QImage>

class QRhi;
class QRhiTexture;
class QRhiTextureRenderTarget;
class QRhiRenderPassDescriptor;
class QRhiReadbackResult;
class QRhiBuffer;
class QRhiGraphicsPipeline;
class QRhiShaderResourceBindings;
class QOffscreenSurface;

namespace srsp::render {

/** @brief GPU renderer backed by QRhi (D3D11 first, Null fallback). */
class RhiRenderer final : public Renderer {
 public:
  RhiRenderer();
  ~RhiRenderer() override;

  [[nodiscard]] const char* name() const noexcept override { return "rhi"; }
  [[nodiscard]] RendererCapabilities capabilities() const noexcept override;

  bool initialize(void* native_window, const Viewport& viewport, std::string* error) override;
  bool resize(const Viewport& viewport) override;
  void set_camera(const CameraView& camera) override;
  /** @brief Supplies the scene to draw; the backend renders its bodies. */
  void set_scene(const Scene& scene);
  bool render_frame() override;

  /**
   * @brief Renders one frame, optionally reading the colour texture back.
   * @param capture When false the GPU-to-CPU copy is skipped, which is what
   *        the frame-rate benchmark measures.
   */
  bool render_frame(bool capture);
  [[nodiscard]] std::uint64_t frame_count() const noexcept override { return frames_; }
  void shutdown() override;

  /** @brief Backend actually created, e.g. "d3d11" or "null". */
  [[nodiscard]] const std::string& backend_name() const noexcept { return backend_; }

  /** @brief True when a graphics pipeline was created (geometry is drawn). */
  [[nodiscard]] bool has_geometry() const noexcept { return pipeline_ != nullptr; }

  /** @brief Blocks until submitted GPU work completes (benchmarking only). */
  void wait_idle();

  /** @brief Last frame as an image; empty when the backend cannot read back. */
  [[nodiscard]] QImage readback() const;

 private:
  bool create_offscreen();
  void destroy_offscreen();

  std::unique_ptr<QRhi> rhi_;
  std::unique_ptr<QRhiTexture> color_;
  std::unique_ptr<QRhiTextureRenderTarget> target_;
  std::unique_ptr<QRhiRenderPassDescriptor> pass_;
  std::unique_ptr<QRhiReadbackResult> readback_;
  std::unique_ptr<QRhiBuffer> vertex_buffer_;
  std::unique_ptr<QRhiBuffer> index_buffer_;
  std::unique_ptr<QRhiBuffer> uniform_buffer_;
  std::unique_ptr<QRhiBuffer> box_buffer_;
  std::uint32_t box_vertices_ = 0;
  std::unique_ptr<QRhiShaderResourceBindings> bindings_;
  std::unique_ptr<QRhiGraphicsPipeline> pipeline_;
  std::unique_ptr<QOffscreenSurface> fallback_surface_;
  std::string backend_ = "none";
  Viewport viewport_{};
  CameraView camera_{};
  Scene scene_{};
  std::uint32_t vertex_count_ = 0;
  bool initialized_ = false;
  bool gpu_ = false;
  QImage image_;
  std::uint64_t frames_ = 0;
};

/** @brief Returns true when a QRhi device can be created on this machine. */
bool rhi_backend_available();

}  // namespace srsp::render

#endif  // SRSP_RENDER_BACKEND_RHI_HPP
