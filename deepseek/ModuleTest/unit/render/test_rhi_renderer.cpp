/**
 * @file test_rhi_renderer.cpp
 * @brief Contract test for the renderer.rhi skeleton.
 *
 * The backend is a skeleton: QRhi device creation needs backend-specific init
 * params, and passing nullptr crashes on Qt 6.11 (ISSUES P33). Until that is
 * wired, initialize() must fail explicitly instead of crashing, and the
 * renderer must stay inert.
 */
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "rhi_renderer.hpp"

using srsp::render::RhiRenderer;
using srsp::render::Viewport;

TEST_CASE("rhi renderer fails safely until init params are wired", "[render_rhi]") {
  RhiRenderer renderer;
  std::string error;
  CHECK_FALSE(renderer.initialize(nullptr, Viewport{320, 180}, &error));
  CHECK(error.find("init params") != std::string::npos);
  CHECK(std::string(renderer.name()) == "rhi");
  CHECK(renderer.frame_count() == 0u);
  CHECK_FALSE(renderer.render_frame());
}

TEST_CASE("rhi renderer validates the viewport first", "[render_rhi]") {
  RhiRenderer renderer;
  std::string error;
  CHECK_FALSE(renderer.initialize(nullptr, Viewport{0, 0}, &error));
  CHECK(error.find("viewport") != std::string::npos);
}

TEST_CASE("rhi renderer reports its capabilities", "[render_rhi]") {
  RhiRenderer renderer;
  const auto capabilities = renderer.capabilities();
  CHECK(capabilities.gpu == false);
  CHECK(capabilities.compute == false);
  CHECK(capabilities.max_viewports == 1);
}