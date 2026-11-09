/**
 * @file capture.hpp
 * @brief Defines what a renderer capture contains (A4 contract).
 *
 * The hybrid text overlay is composited by the window (studio), not by the
 * renderer: `QtRenderer::render_image()` and `RhiRenderer::readback()` return
 * the 3D scene only. Sensor frames and offscreen captures therefore never
 * include UI text, and a future pure-RHI text path must keep that property or
 * this contract must change with a matching test update.
 */
#ifndef SRSP_RENDER_CAPTURE_HPP
#define SRSP_RENDER_CAPTURE_HPP

namespace srsp::render {

/** @brief What a backend capture covers. */
enum class CaptureScope {
  kRendererOnly = 0,     ///< 3D scene only; UI overlay is composited elsewhere.
  kWindowComposite = 1,  ///< Scene plus window overlay (never produced by a backend).
};

/** @brief Every backend capture is renderer-only (A4). */
inline constexpr CaptureScope kRendererCaptureScope = CaptureScope::kRendererOnly;

/** @brief Stable name used in diagnostics and tests. */
[[nodiscard]] constexpr const char* to_string(CaptureScope scope) noexcept {
  switch (scope) {
    case CaptureScope::kRendererOnly:
      return "renderer-only";
    case CaptureScope::kWindowComposite:
      return "window-composite";
  }
  return "unknown";
}

/** @brief Contract check used by the renderer tests and the capability matrix. */
[[nodiscard]] constexpr bool capture_excludes_overlay(CaptureScope scope) noexcept {
  return scope == CaptureScope::kRendererOnly;
}

}  // namespace srsp::render

#endif  // SRSP_RENDER_CAPTURE_HPP
