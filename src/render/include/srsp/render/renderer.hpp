/**
 * @file renderer.hpp
 * @brief Minimal in-process renderer contract used by the studio shell.
 *
 * The full plugin ABI (`srsp.render/renderer@1`) wraps this surface with the
 * usual struct_size/handshake rules; the kernel keeps this C++ interface so the
 * shell can mount any renderer without touching plugin loading. v1 ships the
 * null renderer (headless/CI) and, once bgfx is available, a bgfx renderer.
 */
#ifndef SRSP_RENDER_RENDERER_HPP
#define SRSP_RENDER_RENDERER_HPP

#include <cstdint>
#include <memory>
#include <string>

namespace srsp::render {

/** @brief Framebuffer size in pixels. */
struct Viewport {
  int width = 0;
  int height = 0;
};

/** @brief Perspective camera description (world frame, metres). */
struct CameraView {
  double eye[3] = {0.0, 0.0, 0.0};
  double target[3] = {0.0, 0.0, 0.0};
  double up[3] = {0.0, 0.0, 1.0};
  double fov_deg = 45.0;
  double near_m = 0.01;
  double far_m = 1.0e9;
};

/**
 * @brief Interface every renderer implements.
 *
 * Lifecycle: initialize (once) -> resize/set_camera/render_frame (many) ->
 * shutdown (once, also called by the destructor).
 */
/** @brief Declared capabilities of a renderer backend (reserved for 1.1). */
struct RendererCapabilities {
  const char* backend = "unknown";  ///< "null", "qt", "rhi", "vulkan", ...
  bool gpu = false;                 ///< True when rendering uses a GPU device.
  bool compute = false;             ///< True when the device can also run compute.
  int max_viewports = 1;            ///< Independent viewports per frame.
};
class Renderer {
 public:
  virtual ~Renderer() = default;

  /**
   * @brief Declares backend capabilities.
   *
   * Reservations (see post-M1 upgrade plan R1): a GPU backend reports
   * gpu=true and may set compute=true when it shares its device with the
   * compute layer. The default keeps every existing backend valid.
   */
  [[nodiscard]] virtual RendererCapabilities capabilities() const noexcept {
    return RendererCapabilities{name(), false, false, 1};
  }

  /** @brief Stable renderer name, e.g. "null" or "bgfx". */
  [[nodiscard]] virtual const char* name() const noexcept = 0;

  /**
   * @brief Binds the renderer to a native window.
   * @param native_window HWND on Windows, xcb_window_t on Linux; may be null
   *        for headless operation.
   * @param viewport Initial framebuffer size.
   * @param error Receives a message when the call fails.
   */
  virtual bool initialize(void* native_window, const Viewport& viewport, std::string* error) = 0;

  /** @brief Updates the framebuffer size; must be positive. */
  virtual bool resize(const Viewport& viewport) = 0;

  /** @brief Sets the camera used by the next frame. */
  virtual void set_camera(const CameraView& camera) = 0;

  /** @brief Renders one frame; returns false when the backend failed. */
  virtual bool render_frame() = 0;

  /** @brief Frames rendered since initialisation. */
  [[nodiscard]] virtual std::uint64_t frame_count() const noexcept = 0;

  /** @brief Releases backend resources; idempotent. */
  virtual void shutdown() = 0;
};

/** @brief Headless renderer: validates the call sequence, draws nothing. */
std::unique_ptr<Renderer> make_null_renderer();

}  // namespace srsp::render

#endif  // SRSP_RENDER_RENDERER_HPP
