/**
 * @file test_rhi_gltf.cpp
 * @brief The RHI backend must accept a glTF mesh scene without falling over (B5).
 *
 * Pixel comparisons only make sense on a GPU backend, so the assertions here are
 * that the mesh is loaded, the frame renders and the backend reports success.
 */
#include <srsp/render/capture.hpp>
#include <srsp/render/scene.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <fstream>

#include "rhi_renderer.hpp"

using srsp::render::CameraView;
using srsp::render::RhiRenderer;
using srsp::render::Scene;
using srsp::render::Sphere;
using srsp::render::Viewport;

namespace {

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

std::string write_triangle_glb() {
  const float positions[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  const std::uint16_t indices[3] = {0, 1, 2};
  std::vector<std::uint8_t> bin;
  const auto* p = reinterpret_cast<const std::uint8_t*>(positions);
  bin.insert(bin.end(), p, p + sizeof(positions));
  const auto* q = reinterpret_cast<const std::uint8_t*>(indices);
  bin.insert(bin.end(), q, q + sizeof(indices));
  while (bin.size() % 4u != 0u) {
    bin.push_back(0);
  }
  const std::string json =
      R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":42}],)"
      R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},)"
      R"({"buffer":0,"byteOffset":36,"byteLength":6}],)"
      R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},)"
      R"({"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],)"
      R"("meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":4}]}]})";
  std::vector<std::uint8_t> json_chunk(json.begin(), json.end());
  while (json_chunk.size() % 4u != 0u) {
    json_chunk.push_back(' ');
  }
  const std::uint32_t total = 12u + 8u + static_cast<std::uint32_t>(json_chunk.size()) + 8u +
                              static_cast<std::uint32_t>(bin.size());
  std::vector<std::uint8_t> glb;
  append_u32(glb, 0x46546C67u);
  append_u32(glb, 2u);
  append_u32(glb, total);
  append_u32(glb, static_cast<std::uint32_t>(json_chunk.size()));
  append_u32(glb, 0x4E4F534Au);
  glb.insert(glb.end(), json_chunk.begin(), json_chunk.end());
  append_u32(glb, static_cast<std::uint32_t>(bin.size()));
  append_u32(glb, 0x004E4942u);
  glb.insert(glb.end(), bin.begin(), bin.end());

  const std::string path = (std::filesystem::temp_directory_path() / "srsp_rhi.glb").string();
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(glb.data()), static_cast<std::streamsize>(glb.size()));
  return path;
}

}  // namespace

TEST_CASE("RHI renders a glTF mesh scene", "[render_rhi][gltf]") {
  const std::string path = write_triangle_glb();

  RhiRenderer renderer;
  const Viewport viewport{320, 200};
  std::string error;
  if (!renderer.initialize(nullptr, viewport, &error)) {
    SKIP("RHI backend unavailable: " << error);
  }
  // A4: a backend must not report a capture before the first frame exists.
  CHECK(srsp::render::capture_excludes_overlay(srsp::render::kRendererCaptureScope));
  CHECK(renderer.readback().isNull());

  CameraView camera{};
  camera.eye[2] = -4.0;
  camera.up[2] = 1.0;
  camera.fov_deg = 45.0;
  camera.far_m = 1.0e9;
  renderer.set_camera(camera);

  Scene scene{};
  Sphere body{};
  body.radius_m = 1.0;
  body.color[0] = 0.8f;
  body.mesh_uri = path;
  body.orientation[3] = 1.0;
  scene.bodies.push_back(body);
  renderer.set_scene(scene);
  REQUIRE(renderer.resize(viewport));
  CHECK(renderer.render_frame(true));
  CHECK(renderer.frame_count() == 1u);

  std::filesystem::remove(path);
  renderer.shutdown();
}
TEST_CASE("RHI draws the star field when no body occludes it", "[render_rhi][stars]") {
  RhiRenderer renderer;
  const Viewport viewport{320, 200};
  std::string error;
  if (!renderer.initialize(nullptr, viewport, &error)) {
    SKIP("RHI backend unavailable: " << error);
  }
  if (renderer.backend_name() != "d3d11") {
    SKIP("star points need a GPU backend, backend=" << renderer.backend_name());
  }
  CameraView camera{};
  camera.eye[2] = -6.0;
  camera.up[2] = 1.0;
  camera.fov_deg = 45.0;
  camera.far_m = 1.0e9;
  renderer.set_camera(camera);
  Scene empty{};
  renderer.set_scene(empty);
  REQUIRE(renderer.resize(viewport));
  REQUIRE(renderer.render_frame(true));

  const QImage image = renderer.readback();
  REQUIRE(!image.isNull());
  int bright = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (image.pixelColor(x, y).value() > 60) {
        ++bright;
      }
    }
  }
  std::printf("rhi star pixels=%d\n", bright);
  CHECK(bright > 50);
  renderer.shutdown();
}
