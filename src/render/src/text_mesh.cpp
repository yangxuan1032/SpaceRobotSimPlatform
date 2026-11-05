/**
 * @file text_mesh.cpp
 * @brief Builds screen-space quads with atlas UVs.
 */
#include <srsp/render/text_mesh.hpp>

namespace srsp::render {
namespace {

void push_vertex(std::vector<float>* out, float x, float y, float u, float v, std::uint8_t color) {
  out->push_back(x);
  out->push_back(y);
  out->push_back(u);
  out->push_back(v);
  out->push_back(static_cast<float>(color));
}

}  // namespace

std::size_t build_text_quads(const TextAtlas& atlas, const std::string& text, float x, float y,
                             float scale, std::uint8_t color, std::vector<float>* out) {
  if (!atlas.valid() || out == nullptr) {
    return 0;
  }
  const float glyph_w = static_cast<float>(kGlyphWidth) * scale;
  const float glyph_h = static_cast<float>(kGlyphHeight) * scale;
  const float advance = glyph_w + scale;  // one texel of spacing

  std::size_t emitted = 0;
  float pen = x;
  for (const char character : text) {
    const int code = static_cast<unsigned char>(character);
    if (code < kGlyphFirst || code > kGlyphLast) {
      continue;
    }
    const int index = code - kGlyphFirst;
    const int cell_x = (index % atlas.columns) * kGlyphWidth;
    const int cell_y = (index / atlas.columns) * kGlyphHeight;
    const float u0 = static_cast<float>(cell_x) / static_cast<float>(atlas.width);
    const float v0 = static_cast<float>(cell_y) / static_cast<float>(atlas.height);
    const float u1 = static_cast<float>(cell_x + kGlyphWidth) / static_cast<float>(atlas.width);
    const float v1 = static_cast<float>(cell_y + kGlyphHeight) / static_cast<float>(atlas.height);

    // Two triangles, clockwise, matching a top-left origin.
    push_vertex(out, pen, y, u0, v0, color);
    push_vertex(out, pen + glyph_w, y, u1, v0, color);
    push_vertex(out, pen + glyph_w, y + glyph_h, u1, v1, color);
    push_vertex(out, pen, y, u0, v0, color);
    push_vertex(out, pen + glyph_w, y + glyph_h, u1, v1, color);
    push_vertex(out, pen, y + glyph_h, u0, v1, color);

    pen += advance;
    ++emitted;
  }
  return emitted;
}

}  // namespace srsp::render