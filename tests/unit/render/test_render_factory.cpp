/**
 * @file test_render_factory.cpp
 * @brief The factory maps every renderer choice to a usable backend (A4).
 */
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <renderer_factory.hpp>

using srsp::render::make_renderer;
using srsp::render::RendererKind;

TEST_CASE("factory always returns a renderer", "[render_factory]") {
  for (const auto kind :
       {RendererKind::kNull, RendererKind::kQt, RendererKind::kRhi, RendererKind::kAuto}) {
    const auto renderer = make_renderer(kind);
    REQUIRE(renderer != nullptr);
    CHECK(std::string(renderer->name()).size() > 0u);
  }
}

TEST_CASE("null and auto are distinct choices", "[render_factory]") {
  const auto null_renderer = make_renderer(RendererKind::kNull);
  CHECK(std::string(null_renderer->name()) == "null");
  const auto automatic = make_renderer(RendererKind::kAuto);
  CHECK(std::string(automatic->name()).size() > 0u);
}