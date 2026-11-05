/**
 * @file test_text_atlas.cpp
 * @brief Glyph atlas layout and a couple of known glyphs (B12 step 1).
 */
#include <srsp/render/text_atlas.hpp>

#include <catch2/catch_test_macros.hpp>

using srsp::render::build_text_atlas;
using srsp::render::glyph_row;
using srsp::render::kGlyphHeight;
using srsp::render::kGlyphLast;
using srsp::render::kGlyphWidth;

TEST_CASE("atlas covers the documented range", "[text_atlas]") {
  const auto atlas = build_text_atlas();
  REQUIRE(atlas.valid());
  CHECK(atlas.width == atlas.columns * kGlyphWidth);
  CHECK(atlas.height == 6 * kGlyphHeight);
  const auto cells = (kGlyphLast - 31 + 1);
  CHECK(cells <= atlas.columns * 6);
}

TEST_CASE("known glyphs have the expected pixels", "[text_atlas]") {
  // 'A' has its apex on row 0 and a crossbar on row 3.
  CHECK(glyph_row('A', 0) != 0);
  CHECK(glyph_row('A', 3) != 0);
  CHECK(glyph_row(' ', 0) == 0);
  CHECK(glyph_row(200, 0) == 0);
}