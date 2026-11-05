/**
 * @file mesh_loader.cpp
 * @brief Dependency-free GLB reader for a single triangle primitive.
 *
 * The JSON chunk is parsed with nlohmann (already a project dependency); only the
 * fields needed for one indexed triangle primitive are read.
 */
#include <srsp/render/mesh_loader.hpp>

#include <cstring>

#include <fstream>
#include <nlohmann/json.hpp>

namespace srsp::render {
namespace {

constexpr std::uint32_t kGlbMagic = 0x46546C67u;   // "glTF"
constexpr std::uint32_t kChunkJson = 0x4E4F534Au;  // "JSON"
constexpr std::uint32_t kChunkBin = 0x004E4942u;   // "BIN\0"

constexpr int kComponentUnsignedByte = 5121;
constexpr int kComponentUnsignedShort = 5123;
constexpr int kComponentUnsignedInt = 5125;
constexpr int kComponentFloat = 5126;
constexpr int kModeTriangles = 4;

std::uint32_t read_u32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

struct BufferView {
  std::size_t offset = 0;
  std::size_t length = 0;
};

}  // namespace

MeshLoadResult parse_glb(const std::vector<std::uint8_t>& bytes) {
  MeshLoadResult result;
  if (bytes.size() < 20) {
    result.error = "file is too small to be a GLB";
    return result;
  }
  if (read_u32(bytes.data()) != kGlbMagic) {
    result.error = "missing glTF magic";
    return result;
  }
  if (read_u32(bytes.data() + 4) != 2u) {
    result.error = "only glTF 2.0 is supported";
    return result;
  }

  const std::uint8_t* json_chunk = nullptr;
  std::size_t json_size = 0;
  const std::uint8_t* bin_chunk = nullptr;
  std::size_t bin_size = 0;

  std::size_t offset = 12;
  while (offset + 8 <= bytes.size()) {
    const std::uint32_t length = read_u32(bytes.data() + offset);
    const std::uint32_t type = read_u32(bytes.data() + offset + 4);
    offset += 8;
    if (offset + length > bytes.size()) {
      result.error = "truncated GLB chunk";
      return result;
    }
    if (type == kChunkJson && json_chunk == nullptr) {
      json_chunk = bytes.data() + offset;
      json_size = length;
    } else if (type == kChunkBin && bin_chunk == nullptr) {
      bin_chunk = bytes.data() + offset;
      bin_size = length;
    }
    offset += length;
  }
  if (json_chunk == nullptr) {
    result.error = "missing JSON chunk";
    return result;
  }
  if (bin_chunk == nullptr) {
    result.error = "missing BIN chunk (external buffers are not supported)";
    return result;
  }

  const std::string json_text(reinterpret_cast<const char*>(json_chunk), json_size);
  const nlohmann::json document = nlohmann::json::parse(json_text, nullptr, false);
  if (document.is_discarded()) {
    result.error = "JSON chunk is not valid JSON";
    return result;
  }

  const auto& buffers = document.at("buffers");
  if (buffers.empty() || buffers[0].value("uri", std::string{}) != "") {
    result.error = "buffers[0] must be the embedded BIN chunk";
    return result;
  }

  std::vector<BufferView> views;
  for (const auto& view : document.at("bufferViews")) {
    BufferView entry;
    entry.offset = view.value("byteOffset", 0u);
    entry.length = view.value("byteLength", 0u);
    views.push_back(entry);
  }

  const auto& meshes = document.at("meshes");
  if (meshes.empty() || meshes[0].at("primitives").empty()) {
    result.error = "mesh 0 has no primitives";
    return result;
  }
  const auto& primitive = meshes[0].at("primitives")[0];
  if (primitive.value("mode", kModeTriangles) != kModeTriangles) {
    result.error = "only triangle primitives are supported";
    return result;
  }

  const auto& accessors = document.at("accessors");
  const std::size_t position_accessor =
      primitive.at("attributes").at("POSITION").get<std::size_t>();
  if (position_accessor >= accessors.size()) {
    result.error = "POSITION accessor index is out of range";
    return result;
  }
  const auto& position = accessors[position_accessor];
  if (position.value("componentType", 0) != kComponentFloat ||
      position.value("type", std::string{}) != "VEC3") {
    result.error = "POSITION must be VEC3 float";
    return result;
  }
  const std::size_t count = position.value("count", 0u);
  if (count == 0u) {
    result.error = "POSITION accessor is empty";
    return result;
  }
  const std::size_t position_view_index = position.at("bufferView").get<std::size_t>();
  if (position_view_index >= views.size()) {
    result.error = "POSITION bufferView is out of range";
    return result;
  }
  const BufferView& position_view = views[position_view_index];
  const std::size_t position_offset = position_view.offset + position.value("byteOffset", 0u);
  const std::size_t position_bytes = count * 3u * sizeof(float);
  if (position_offset + position_bytes > bin_size) {
    result.error = "POSITION data is out of bounds";
    return result;
  }
  result.mesh.positions.resize(count * 3u);
  std::memcpy(result.mesh.positions.data(), bin_chunk + position_offset, position_bytes);

  if (!primitive.contains("indices")) {
    result.mesh.indices.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
      result.mesh.indices[i] = static_cast<std::uint32_t>(i);
    }
    return result;
  }

  const std::size_t index_accessor = primitive.at("indices").get<std::size_t>();
  if (index_accessor >= accessors.size()) {
    result.error = "index accessor index is out of range";
    return result;
  }
  const auto& indices = accessors[index_accessor];
  const int index_type = indices.value("componentType", 0);
  if (index_type != kComponentUnsignedByte && index_type != kComponentUnsignedShort &&
      index_type != kComponentUnsignedInt) {
    result.error = "indices must be unsigned byte, short or int";
    return result;
  }
  const std::size_t index_count = indices.value("count", 0u);
  const std::size_t index_view_index = indices.at("bufferView").get<std::size_t>();
  if (index_view_index >= views.size()) {
    result.error = "index bufferView is out of range";
    return result;
  }
  const BufferView& index_view = views[index_view_index];
  const std::size_t index_offset = index_view.offset + indices.value("byteOffset", 0u);
  const std::size_t element =
      index_type == kComponentUnsignedByte ? 1u : (index_type == kComponentUnsignedShort ? 2u : 4u);
  if (index_offset + index_count * element > bin_size) {
    result.error = "index data is out of bounds";
    return result;
  }
  result.mesh.indices.resize(index_count);
  for (std::size_t i = 0; i < index_count; ++i) {
    const std::uint8_t* p = bin_chunk + index_offset + i * element;
    std::uint32_t value = 0;
    if (element == 1u) {
      value = p[0];
    } else if (element == 2u) {
      value = static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8);
    } else {
      value = read_u32(p);
    }
    if (value >= count) {
      result.error = "index is out of range";
      return result;
    }
    result.mesh.indices[i] = value;
  }
  return result;
}

MeshLoadResult load_glb(const std::string& path) {
  MeshLoadResult result;
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    result.error = "cannot open " + path;
    return result;
  }
  const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
  return parse_glb(bytes);
}

}  // namespace srsp::render
