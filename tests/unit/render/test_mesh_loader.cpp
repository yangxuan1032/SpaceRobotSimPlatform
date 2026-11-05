/**
 * @file test_mesh_loader.cpp
 * @brief GLB parsing: good input, bad magic, truncation (B5).
 */
#include <srsp/render/mesh_loader.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using srsp::render::parse_glb;

namespace {

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

/// Builds a minimal one-triangle GLB: three float3 positions and three u16 indices.
std::vector<std::uint8_t> make_triangle_glb() {
  const float positions[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  const std::uint16_t indices[3] = {0, 1, 2};

  std::vector<std::uint8_t> bin;
  const auto* pos_bytes = reinterpret_cast<const std::uint8_t*>(positions);
  bin.insert(bin.end(), pos_bytes, pos_bytes + sizeof(positions));
  const auto* idx_bytes = reinterpret_cast<const std::uint8_t*>(indices);
  bin.insert(bin.end(), idx_bytes, idx_bytes + sizeof(indices));
  while (bin.size() % 4u != 0u) {
    bin.push_back(0);
  }

  const std::string json =
      R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":36}],)"
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
  return glb;
}

}  // namespace

TEST_CASE("a minimal GLB yields positions and indices", "[mesh_loader]") {
  const auto bytes = make_triangle_glb();
  const auto result = parse_glb(bytes);
  REQUIRE(result.ok());
  CHECK(result.mesh.positions.size() == 9u);
  CHECK(result.mesh.indices.size() == 3u);
  CHECK(result.mesh.indices[1] == 1u);
}

TEST_CASE("bad magic and truncated GLB are rejected", "[mesh_loader]") {
  auto bytes = make_triangle_glb();
  bytes[0] = 0;
  CHECK_FALSE(parse_glb(bytes).ok());

  const auto good = make_triangle_glb();
  const std::vector<std::uint8_t> truncated(good.begin(), good.begin() + 24);
  CHECK_FALSE(parse_glb(truncated).ok());
}