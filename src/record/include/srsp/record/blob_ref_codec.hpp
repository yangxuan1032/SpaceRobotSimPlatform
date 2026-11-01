/**
 * @file blob_ref_codec.hpp
 * @brief Wire encoding for the `sim.blobref` channel (D4).
 *
 * Layout (little endian, 28 bytes): u64 id, u64 generation, u64 size_bytes,
 * u32 flags. Kept explicit rather than memcpy'd so the format does not depend
 * on struct padding, and so a peer written in another language can parse it.
 */
#ifndef SRSP_RECORD_BLOB_REF_CODEC_HPP
#define SRSP_RECORD_BLOB_REF_CODEC_HPP

#include <srsp/core/blob.hpp>

#include <cstdint>

#include <cstddef>

namespace srsp::record {

/** @brief Encoded size of one BlobRef. */
constexpr std::size_t kBlobRefWireSize = 28;

/** @brief Encodes @p ref into @p out (must hold kBlobRefWireSize bytes). */
inline void encode_blob_ref(const srsp::core::BlobRef& ref, std::uint8_t* out) noexcept {
  const auto put = [&out](std::uint64_t value, std::size_t width) {
    for (std::size_t i = 0; i < width; ++i) {
      out[i] = static_cast<std::uint8_t>((value >> (8u * i)) & 0xFFu);
    }
    out += width;
  };
  put(ref.id, 8);
  put(ref.generation, 8);
  put(ref.size_bytes, 8);
  put(ref.flags, 4);
}

/**
 * @brief Decodes a BlobRef from @p data.
 * @param size Must be at least kBlobRefWireSize.
 * @return False on a short buffer or an invalid (zero id/generation) reference.
 */
inline bool decode_blob_ref(const std::uint8_t* data, std::size_t size,
                            srsp::core::BlobRef* out) noexcept {
  if (data == nullptr || out == nullptr || size < kBlobRefWireSize) {
    return false;
  }
  const auto read = [&data](std::size_t width) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < width; ++i) {
      value |= static_cast<std::uint64_t>(data[i]) << (8u * i);
    }
    data += width;
    return value;
  };
  srsp::core::BlobRef ref;
  ref.id = read(8);
  ref.generation = read(8);
  ref.size_bytes = read(8);
  ref.flags = static_cast<std::uint32_t>(read(4));
  if (!ref.valid()) {
    return false;
  }
  *out = ref;
  return true;
}

}  // namespace srsp::record

#endif  // SRSP_RECORD_BLOB_REF_CODEC_HPP
