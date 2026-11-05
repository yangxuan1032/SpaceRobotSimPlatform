/**
 * @file text_mesh.hpp
 * @brief Screen-space quads for a text run (B12, step 2a).
 *
 * Turns a string into 2D triangles with atlas UVs so a backend only has to bind
 * the atlas texture and draw them. Pure CPU, unit-testable without a device.
 */
#ifndef SRSP_RENDER_TEXT_MESH_HPP
#define SRSP_RENDER_TEXT_MESH_HPP

#include <srsp/render/text_atlas.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace srsp::render {

/** @brief One text vertex: position in pixels, UV in atlas space. */
struct TextVertex {
  float x = 0.0f;
  float y = 0.0f;
  float u = 0.0f;
  float v = 0.0f;
};

/**
 * @brief Builds triangles for @p text starting at (@p x, @p y).
 * @param atlas Atlas produced by build_text_atlas().
 * @param text UTF-8 (ASCII subset).
 * @param x Left edge in pixels.
 * @param y Top edge in pixels.
 * @param scale Pixel size of one atlas texel.
 * @param color Greyscale tint written into the vertex colour slots, 0..255.
 * @param out Appends six vertices per character.
 * @return Number of characters emitted (unsupported ones are skipped).
 */
[[nodiscard]] std::size_t build_text_quads(const TextAtlas& atlas, const std::string& text, float x,
                                           float y, float scale, std::uint8_t color,
                                           std::vector<float>* out);

}  // namespace srsp::render

#endif  // SRSP_RENDER_TEXT_MESH_HPP