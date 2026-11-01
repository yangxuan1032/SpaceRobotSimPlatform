/**
 * @file blob_recorder.hpp
 * @brief Bridges BlobStore references into MCAP attachments (D4).
 *
 * The recorder owns the policy that the raw writer does not: payloads above the
 * limit are refused with a reason instead of truncating, and the store reference
 * is released only after the attachment and its metadata are on disk.
 */
#ifndef SRSP_RECORD_BLOB_RECORDER_HPP
#define SRSP_RECORD_BLOB_RECORDER_HPP

#include <srsp/core/blob.hpp>
#include <srsp/record/mcap.hpp>

#include <cstdint>
#include <string>

namespace srsp::record {

/** @brief Capacity policy for recorded blobs. */
struct BlobLimits {
  std::uint64_t max_blob_bytes = 256ull * 1024ull * 1024ull;
  std::uint32_t max_blobs = 4096;
};

/** @brief Writes BlobRef payloads as attachments plus a metadata record. */
class BlobRecorder {
 public:
  BlobRecorder(McapWriter& writer, srsp::core::BlobStore& store, BlobLimits limits = {})
      : writer_(writer), store_(store), limits_(limits) {}

  /**
   * @brief Records @p ref's payload at @p log_time_ns.
   * @return False when the writer is closed, the attachment fails, or a limit
   *         is exceeded; @ref error() then explains why.
   */
  bool write(const srsp::core::BlobRef& ref, std::uint64_t log_time_ns) {
    if (!ref.valid()) {
      error_ = "blob reference is not valid";
      return false;
    }
    if (ref.size_bytes > limits_.max_blob_bytes) {
      error_ = "blob exceeds max_blob_bytes";
      return false;
    }
    if (count_ >= limits_.max_blobs) {
      error_ = "blob count exceeds max_blobs";
      return false;
    }
    const auto lease = store_.acquire(ref);
    if (!lease.has_value()) {
      error_ = "blob reference is stale or unknown";
      return false;
    }

    const std::string name = "blob/" + std::to_string(ref.id);
    if (!writer_.add_attachment(name, log_time_ns, "application/octet-stream", (*lease)->data(),
                                (*lease)->size())) {
      error_ = "attachment write failed";
      return false;
    }
    if (!writer_.add_metadata(name, {{"blob_generation", std::to_string(ref.generation)},
                                     {"blob_size", std::to_string(ref.size_bytes)}})) {
      error_ = "metadata write failed";
      return false;
    }
    store_.release(ref);
    ++count_;
    return true;
  }

  [[nodiscard]] const std::string& error() const noexcept { return error_; }
  [[nodiscard]] std::uint32_t count() const noexcept { return count_; }

 private:
  McapWriter& writer_;
  srsp::core::BlobStore& store_;
  BlobLimits limits_;
  std::string error_;
  std::uint32_t count_ = 0;
};

}  // namespace srsp::record

#endif  // SRSP_RECORD_BLOB_RECORDER_HPP
