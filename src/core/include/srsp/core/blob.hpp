/**
 * @file blob.hpp
 * @brief BlobRef: generation-checked references to immutable binary payloads.
 *
 * Large payloads (images, point clouds, recording excerpts) are not copied
 * through the ABI. Producers park the bytes in a BlobStore and pass a small
 * BlobRef, as described in docs/02 section 17. A reference carries the store
 * generation so a consumer that arrives late cannot silently read a payload
 * that has been reused or released.
 */
#ifndef SRSP_CORE_BLOB_HPP
#define SRSP_CORE_BLOB_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <cstddef>

namespace srsp::core {

/** @brief Immutable payload bytes shared by producer and consumers. */
using BlobBytes = std::vector<std::uint8_t>;

/** @brief Opaque reference to a stored payload. */
struct BlobRef {
  std::uint64_t id = 0;          ///< Store-assigned identity.
  std::uint64_t generation = 0;  ///< Must match the stored generation.
  std::uint64_t size_bytes = 0;  ///< Payload size, for capacity decisions.
  std::uint32_t flags = 0;       ///< Reserved; must be zero in 1.x.

  [[nodiscard]] bool valid() const noexcept { return id != 0 && generation != 0; }
};

/**
 * @brief Owns payloads and hands out shared, immutable views.
 *
 * A lease is a `shared_ptr<const BlobBytes>`: the bytes stay alive while any
 * consumer holds one, even after the store releases its reference.
 */
class BlobStore {
 public:
  /** @brief Stores a copy and returns its reference. */
  BlobRef store(BlobBytes bytes) {
    const std::uint64_t id = next_id_++;
    const std::uint64_t generation = ++generation_;
    stored_bytes_ += bytes.size();
    entries_.emplace(id, Entry{generation, std::make_shared<const BlobBytes>(std::move(bytes))});
    return BlobRef{id, generation, entries_.at(id).bytes->size(), 0};
  }

  /**
   * @brief Acquires a lease.
   * @return Empty when the id is unknown or the generation is stale.
   */
  [[nodiscard]] std::optional<std::shared_ptr<const BlobBytes>> acquire(const BlobRef& ref) const {
    const auto it = entries_.find(ref.id);
    if (it == entries_.end() || it->second.generation != ref.generation) {
      return std::nullopt;
    }
    return it->second.bytes;
  }

  /** @brief Drops the store's reference; outstanding leases stay valid. */
  void release(const BlobRef& ref) {
    const auto it = entries_.find(ref.id);
    if (it != entries_.end() && it->second.generation == ref.generation) {
      stored_bytes_ -= it->second.bytes->size();
      entries_.erase(it);
    }
  }

  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
  [[nodiscard]] std::uint64_t stored_bytes() const noexcept { return stored_bytes_; }

 private:
  struct Entry {
    std::uint64_t generation = 0;
    std::shared_ptr<const BlobBytes> bytes;
  };

  std::map<std::uint64_t, Entry> entries_;
  std::uint64_t next_id_ = 1;
  std::uint64_t generation_ = 0;
  std::uint64_t stored_bytes_ = 0;
};

}  // namespace srsp::core

#endif  // SRSP_CORE_BLOB_HPP
