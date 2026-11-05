/**
 * @file mesh_loader.hpp
 * @brief Minimal glTF/GLB triangle-mesh reader (B5, first step).
 *
 * Only the subset SRSP needs today: a GLB container with one primitive whose
 * POSITION accessor is float3 and whose indices are unsigned short/int. The
 * reader is internal and dependency-free; a full glTF implementation (materials,
 * skinning, external buffers) is a later step.
 */
#ifndef SRSP_RENDER_MESH_LOADER_HPP
#define SRSP_RENDER_MESH_LOADER_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace srsp::render {

/** @brief A triangle mesh in model space. */
struct MeshData {
  std::vector<float> positions;        ///< xyz triples.
  std::vector<std::uint32_t> indices;  ///< Triangle indices.
  [[nodiscard]] bool empty() const noexcept { return positions.empty(); }
};

/** @brief Result of loading a mesh file. */
struct MeshLoadResult {
  MeshData mesh;
  std::string error;  ///< Empty on success.

  [[nodiscard]] bool ok() const noexcept { return error.empty(); }
};

/**
 * @brief Loads the first triangle primitive of a .glb file.
 * @param path File to read.
 */
[[nodiscard]] MeshLoadResult load_glb(const std::string& path);

/**
 * @brief Parses GLB bytes (kept separate so tests can build tiny assets).
 * @param bytes Whole file contents.
 */
[[nodiscard]] MeshLoadResult parse_glb(const std::vector<std::uint8_t>& bytes);

}  // namespace srsp::render

#endif  // SRSP_RENDER_MESH_LOADER_HPP