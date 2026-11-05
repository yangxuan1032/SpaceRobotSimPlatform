/**
 * @file test_text_mesh.cpp
 * @brief Text quad generation and atlas UV bounds (B12 step 2a).
 */
#include <srsp/render/text_mesh.hpp>

#include <vector>

#include <catch2/catch_test_macros.hpp>

using srsp::render::build_text_atlas;
using srsp::render::build_text_quads;

TEST_CASE("quads are emitted per supported character", "[text_mesh]") {
  const auto atlas = build_text_atlas();
  std::vector<float> vertices;
  const auto emitted = build_text_quads(atlas, "AB", 0.0f, 0.0f, 2.0f, 255, &vertices);
  CHECK(emitted == 2u);
  CHECK(vertices.size() == 2u * 6u * 5u);
}

TEST_CASE("unsupported characters are skipped and UVs stay in range", "[text_mesh]") {
  const auto atlas = build_text_atlas();
  std::vector<float> vertices;
  const auto emitted = build_text_quads(atlas, "A?~", 0.0f, 0.0f, 1.0f, 200, &vertices);
  CHECK(emitted == 2u);  // 'A' and '?' are in range, '~' is not
  for (std::size_t i = 0; i < vertices.size(); i += 5u) {
    CHECK(vertices[i + 2] >= 0.0f);
    CHECK(vertices[i + 2] <= 1.0f);
    CHECK(vertices[i + 3] >= 0.0f);
    CHECK(vertices[i + 3] <= 1.0f);
  }
}