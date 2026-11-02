/**
 * @file mesh_source.hpp
 * @brief Maps a scene mesh URI to the geometry a backend should draw.
 */
#ifndef SRSP_RENDER_MESH_SOURCE_HPP
#define SRSP_RENDER_MESH_SOURCE_HPP

#include <string_view>

namespace srsp::render {

/** @brief Geometry kind selected by a mesh URI. */
enum class MeshSource {
  kSphere = 0,  ///< Procedural UV sphere (default when the URI is empty).
  kBox = 1,     ///< Procedural box ("box").
  kGltf = 2,    ///< glTF asset (reserved).
};

/**
 * @brief Classifies @p uri.
 * @param uri Value of `Sphere::mesh_uri`.
 */
[[nodiscard]] inline MeshSource mesh_source_from_uri(std::string_view uri) noexcept {
  if (uri.empty()) {
    return MeshSource::kSphere;
  }
  if (uri == "box") {
    return MeshSource::kBox;
  }
  return MeshSource::kGltf;
}

/** @brief Stable name used in diagnostics. */
[[nodiscard]] inline const char* to_string(MeshSource source) noexcept {
  switch (source) {
    case MeshSource::kSphere:
      return "sphere";
    case MeshSource::kBox:
      return "box";
    case MeshSource::kGltf:
      return "gltf";
  }
  return "unknown";
}

}  // namespace srsp::render

#endif  // SRSP_RENDER_MESH_SOURCE_HPP