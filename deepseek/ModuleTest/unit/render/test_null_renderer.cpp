/**
 * @file test_null_renderer.cpp
 * @brief Unit tests for the renderer contract using the null backend.
 */
#include <srsp/render/renderer.hpp>

#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

using srsp::render::CameraView;
using srsp::render::make_null_renderer;
using srsp::render::Viewport;

TEST_CASE("null renderer follows the lifecycle", "[render]") {
  std::unique_ptr<srsp::render::Renderer> renderer = make_null_renderer();
  REQUIRE(std::string(renderer->name()) == "null");
  CHECK(renderer->frame_count() == 0u);

  std::string error;
  REQUIRE(renderer->initialize(nullptr, Viewport{800, 600}, &error));
  CHECK(error.empty());
  CHECK(renderer->render_frame());
  CHECK(renderer->frame_count() == 1u);

  renderer->shutdown();
  CHECK(renderer->frame_count() == 0u);
  CHECK_FALSE(renderer->render_frame());
}

TEST_CASE("null renderer validates the viewport", "[render]") {
  std::unique_ptr<srsp::render::Renderer> renderer = make_null_renderer();
  std::string error;
  CHECK_FALSE(renderer->initialize(nullptr, Viewport{0, 600}, &error));
  CHECK(error.find("viewport") != std::string::npos);

  REQUIRE(renderer->initialize(nullptr, Viewport{640, 480}, nullptr));
  CHECK_FALSE(renderer->resize(Viewport{-1, 10}));
  CHECK(renderer->resize(Viewport{1920, 1080}));
}

TEST_CASE("null renderer rejects a second initialisation", "[render]") {
  std::unique_ptr<srsp::render::Renderer> renderer = make_null_renderer();
  REQUIRE(renderer->initialize(nullptr, Viewport{320, 240}, nullptr));
  std::string error;
  CHECK_FALSE(renderer->initialize(nullptr, Viewport{320, 240}, &error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("null renderer accepts a camera and a native handle", "[render]") {
  std::unique_ptr<srsp::render::Renderer> renderer = make_null_renderer();
  int fake_window = 0;
  REQUIRE(renderer->initialize(&fake_window, Viewport{1024, 768}, nullptr));

  CameraView camera;
  camera.eye[0] = 1.0;
  camera.target[2] = 5.0;
  renderer->set_camera(camera);

  CHECK(renderer->render_frame());
  CHECK(renderer->frame_count() == 1u);
}
