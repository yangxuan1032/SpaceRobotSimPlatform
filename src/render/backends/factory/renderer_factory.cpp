/**
 * @file renderer_factory.cpp
 * @brief Backend selection; each backend stays behind its own target.
 */
#include "renderer_factory.hpp"

#include <srsp/render/renderer.hpp>

#ifdef SRSP_RENDER_FACTORY_HAVE_QT
#include <qt_renderer.hpp>
#endif
#ifdef SRSP_RENDER_FACTORY_HAVE_RHI
#include <rhi_renderer.hpp>
#endif

namespace srsp::render {

std::unique_ptr<Renderer> make_renderer(RendererKind kind) {
  switch (kind) {
#ifdef SRSP_RENDER_FACTORY_HAVE_RHI
    case RendererKind::kRhi:
      return std::make_unique<srsp::render::RhiRenderer>();
#endif
#ifdef SRSP_RENDER_FACTORY_HAVE_QT
    case RendererKind::kQt:
      return std::make_unique<srsp::studio::QtRenderer>();
#endif
    case RendererKind::kAuto:
#ifdef SRSP_RENDER_FACTORY_HAVE_RHI
      return std::make_unique<srsp::render::RhiRenderer>();
#elif defined(SRSP_RENDER_FACTORY_HAVE_QT)
      return std::make_unique<srsp::studio::QtRenderer>();
#else
      return srsp::render::make_null_renderer();
#endif
    case RendererKind::kNull:
      return srsp::render::make_null_renderer();
    default:
      // A requested backend that was not built must not silently degrade: the
      // caller compares names and reports SRSP_E_UNSUPPORTED.
      return srsp::render::make_null_renderer();
  }
}

}  // namespace srsp::render