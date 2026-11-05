/**
 * @file text_atlas.hpp
 * @brief CPU glyph atlas for in-pipeline text (B12, first step).
 *
 * A 5x7 bitmap font for printable ASCII is rasterised into a single 8-bit
 * coverage atlas. The RHI backend can upload it as an R8 texture and sample it
 * from a plate shader, which removes the QPainter dependency from the frame.
 */
#ifndef SRSP_RENDER_TEXT_ATLAS_HPP
#define SRSP_RENDER_TEXT_ATLAS_HPP

#include <cstdint>
#include <vector>

namespace srsp::render {

/** @brief Fixed glyph grid. */
inline constexpr int kGlyphWidth = 5;
inline constexpr int kGlyphHeight = 7;
inline constexpr int kGlyphFirst = 32;  ///< Space.
inline constexpr int kGlyphLast = 90;   ///< Z (lowercase and symbols are pending).

/** @brief Atlas image plus layout metadata. */
struct TextAtlas {
  int width = 0;                     ///< Pixels per row of glyphs.
  int height = 0;                    ///< Pixels per column of glyphs.
  int columns = 0;                   ///< Glyphs per row.
  std::vector<std::uint8_t> pixels;  ///< 8-bit coverage, row major.

  /** @brief True when the atlas has been built. */
  [[nodiscard]] bool valid() const noexcept { return width > 0 && !pixels.empty(); }
};

/** @brief Builds a 16-column atlas covering ASCII 32..126. */
[[nodiscard]] TextAtlas build_text_atlas();

/**
 * @brief Returns the bitmap of @p character (7 rows, 5 bits each, bit 4 = left).
 * @param character Printable ASCII; out-of-range characters map to a blank cell.
 */
[[nodiscard]] std::uint8_t glyph_row(int character, int row);

}  // namespace srsp::render

#endif  // SRSP_RENDER_TEXT_ATLAS_HPP