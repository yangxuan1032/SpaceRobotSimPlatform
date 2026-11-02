/**
 * @file renderer_factory.hpp
 * @brief Single place that maps a renderer choice to a concrete backend.
 *
 * Applications should not include the backend headers directly: selecting a
 * renderer is a runtime decision (docs/02 section 15.1, `config.renderer`).
 */
#ifndef SRSP_RENDER_FACTORY_HPP
#define SRSP_RENDER_FACTORY_HPP

#include <srsp/render/renderer.hpp>

#include <cstdint>
#include <memory>

namespace srsp::render {

/** @brief Backend choice, mirroring `srsp_context_config.renderer`. */
enum class RendererKind : std::uint32_t {
  kAuto = 0,  ///< Prefer a GPU backend, fall back to Qt, then Null.
  kQt = 1,    ///< QPainter software backend.
  kRhi = 2,   ///< QRhi backend (D3D11 when available).
  kNull = 3,  ///< Headless backend that validates the call sequence.
};

/**
 * @brief Creates a renderer for @p kind.
 * @return Never null: unknown or unavailable kinds fall back to Null.
 */
[[nodiscard]] std::unique_ptr<Renderer> make_renderer(RendererKind kind);

}  // namespace srsp::render

#endif  // SRSP_RENDER_FACTORY_HPP