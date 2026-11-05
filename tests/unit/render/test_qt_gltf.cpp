/**
 * @file test_qt_gltf.cpp
 * @brief A glTF mesh URI must draw the loaded geometry (B5).
 *
 * Builds a small GLB on disk, renders it through the Qt backend and compares the
 * result with the sphere fallback: the two images must differ, otherwise the URI
 * was ignored.
 */
#include <srsp/render/scene.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <QColor>
#include <QImage>
#include <catch2/catch_test_macros.hpp>
#include <fstream>

#include "qt_renderer.hpp"

using srsp::render::CameraView;
using srsp::render::Scene;
using srsp::render::Sphere;
using srsp::render::Viewport;
using srsp::studio::QtRenderer;

namespace {

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

/// A 1 m square (two triangles) centred on the body origin.
std::string write_quad_glb() {
  const float positions[12] = {-0.5f, -0.5f, 0.0f, 0.5f,  -0.5f, 0.0f,
                               0.5f,  0.5f,  0.0f, -0.5f, 0.5f,  0.0f};
  const std::uint16_t indices[6] = {0, 1, 2, 0, 2, 3};

  std::vector<std::uint8_t> bin;
  const auto* p = reinterpret_cast<const std::uint8_t*>(positions);
  bin.insert(bin.end(), p, p + sizeof(positions));
  const auto* q = reinterpret_cast<const std::uint8_t*>(indices);
  bin.insert(bin.end(), q, q + sizeof(indices));
  while (bin.size() % 4u != 0u) {
    bin.push_back(0);
  }

  const std::string json =
      R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":54}],)"
      R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},)"
      R"({"buffer":0,"byteOffset":48,"byteLength":12}],)"
      R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},)"
      R"({"bufferView":1,"componentType":5123,"count":6,"type":"SCALAR"}],)"
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

  const std::string path = (std::filesystem::temp_directory_path() / "srsp_quad.glb").string();
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(glb.data()), static_cast<std::streamsize>(glb.size()));
  return path;
}

CameraView make_camera() {
  CameraView camera{};
  camera.eye[1] = -5.0;
  camera.eye[2] = 1.5;
  camera.up[2] = 1.0;
  camera.fov_deg = 45.0;
  camera.near_m = 0.01;
  camera.far_m = 1.0e9;
  return camera;
}

QImage render(const char* mesh_uri) {
  QtRenderer renderer;
  const Viewport viewport{320, 200};
  std::string error;
  REQUIRE(renderer.initialize(nullptr, viewport, &error));
  renderer.set_camera(make_camera());

  Scene scene{};
  Sphere body{};
  body.radius_m = 0.5;
  body.color[0] = 0.9f;
  body.color[1] = 0.4f;
  body.color[2] = 0.2f;
  body.mesh_uri = mesh_uri;
  body.orientation[3] = 1.0;
  scene.bodies.push_back(body);
  renderer.set_scene(scene);
  return renderer.render_image(viewport.width, viewport.height);
}

int count_foreground(const QImage& image) {
  const QColor background(8, 10, 16);
  int count = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (QColor(image.pixel(x, y)) != background) {
        ++count;
      }
    }
  }
  return count;
}

}  // namespace

TEST_CASE("a GLB mesh URI changes what is drawn", "[render_qt][gltf]") {
  const std::string path = write_quad_glb();
  const QImage mesh_image = render(path.c_str());
  const QImage sphere_image = render("");

  const int mesh_pixels = count_foreground(mesh_image);
  const int sphere_pixels = count_foreground(sphere_image);
  std::printf("gltf pixels=%d sphere=%d\n", mesh_pixels, sphere_pixels);

  CHECK(mesh_pixels > 500);
  CHECK(sphere_pixels > 500);

  int diff = 0;
  for (int y = 0; y < mesh_image.height(); ++y) {
    for (int x = 0; x < mesh_image.width(); ++x) {
      if (mesh_image.pixel(x, y) != sphere_image.pixel(x, y)) {
        ++diff;
      }
    }
  }
  CHECK(diff > 500);
  std::filesystem::remove(path);
}