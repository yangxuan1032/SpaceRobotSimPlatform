/**
 * @file mcap.hpp
 * @brief Minimal MCAP writer/reader (Header, Schema, Channel, Message, DataEnd,
 *        Footer) sufficient for M1 recording and replay verification.
 *
 * Deliberately scoped: no compression, no chunking, no summary index and no
 * attachments. Every record is encoded as opcode(1) + length(u64 LE) + payload,
 * with the 8-byte file magic at both ends, which `mcap info`-style tools accept
 * for the subset. The reader validates the magic and walks the records.
 */
#ifndef SRSP_RECORD_MCAP_HPP
#define SRSP_RECORD_MCAP_HPP

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace srsp::record {

/** @brief One recorded message. */
struct Message {
  std::uint32_t channel_id = 0;
  std::uint64_t sequence = 0;
  std::uint64_t log_time_ns = 0;
  std::vector<std::uint8_t> data;
};

/** @brief Schema and channel descriptions stored in the file. */
struct ChannelInfo {
  std::uint32_t id = 0;
  std::uint32_t schema_id = 0;
  std::string topic;
  std::string message_encoding;
  std::string schema_name;
  std::string schema_encoding;
  std::string schema_data;
  std::map<std::string, std::string> metadata;  ///< Channel metadata (schema hash, abi).
};

/** @brief One binary attachment (blob payload). */
struct Attachment {
  std::string name;
  std::string media_type;
  std::uint64_t log_time_ns = 0;
  std::vector<std::uint8_t> data;
};

/** @brief Writes a flat MCAP subset. */
class McapWriter {
 public:
  /**
   * @brief Opens @p path for writing (truncates an existing file).
   * @param profile Optional profile string, e.g. "srsp.replay".
   */
  bool open(const std::string& path, const std::string& profile = "srsp");

  /** @brief Registers a schema and returns its id (1-based). */
  std::uint32_t add_schema(const std::string& name, const std::string& encoding,
                           const std::string& data);

  /** @brief Registers a channel bound to a schema and returns its id (1-based). */
  std::uint32_t add_channel(std::uint32_t schema_id, const std::string& topic,
                            const std::string& message_encoding,
                            const std::map<std::string, std::string>& metadata = {});

  /** @brief Appends a message for an existing channel. */
  bool write_message(std::uint32_t channel_id, std::uint64_t sequence, std::uint64_t log_time_ns,
                     const void* data, std::size_t size);

  /** @brief Writes a binary attachment (used for BlobRef payloads). */
  bool add_attachment(const std::string& name, std::uint64_t log_time_ns,
                      const std::string& media_type, const void* data, std::size_t size);

  /** @brief Writes a metadata record (key/value pairs attached to @p name). */
  bool add_metadata(const std::string& name, const std::map<std::string, std::string>& entries);

  /** @brief Writes DataEnd, the footer and the trailing magic, then closes. */
  bool close();

  [[nodiscard]] std::uint64_t message_count() const noexcept { return messages_; }
  [[nodiscard]] bool is_open() const noexcept { return open_; }

 private:
  bool write_record(std::uint8_t opcode, const std::vector<std::uint8_t>& payload);

  std::string path_;
  struct AttachmentEntry {
    std::uint64_t offset = 0;
    std::uint64_t length = 0;
    std::uint64_t log_time_ns = 0;
    std::uint64_t data_size = 0;
    std::string name;
    std::string media_type;
  };
  struct MetadataEntry {
    std::uint64_t offset = 0;
    std::uint64_t length = 0;
    std::string name;
  };

  struct SchemaEntry {
    std::uint32_t id = 0;
    std::string name;
    std::string encoding;
    std::string data;
  };

  std::vector<std::uint8_t> buffer_;
  std::vector<SchemaEntry> schemas_;
  std::vector<AttachmentEntry> attachment_entries_;
  std::vector<MetadataEntry> metadata_entries_;
  std::uint16_t schema_count_ = 0;
  std::uint64_t first_time_ = 0;
  std::uint64_t last_time_ = 0;
  bool has_time_ = false;
  std::vector<ChannelInfo> channels_;
  std::string profile_;
  std::uint64_t messages_ = 0;
  std::uint32_t pending_schema_id_ = 0;
  std::string pending_schema_name_;
  std::string pending_schema_encoding_;
  bool open_ = false;
  bool error_ = false;
};

/** @brief Reads a flat MCAP subset back. */
class McapReader {
 public:
  bool open(const std::string& path);
  [[nodiscard]] bool is_open() const noexcept { return open_; }
  [[nodiscard]] const std::string& profile() const noexcept { return profile_; }
  [[nodiscard]] const std::vector<ChannelInfo>& channels() const noexcept { return channels_; }
  [[nodiscard]] const std::vector<Message>& messages() const noexcept { return messages_; }
  [[nodiscard]] const std::vector<Attachment>& attachments() const noexcept { return attachments_; }
  [[nodiscard]] const std::map<std::string, std::map<std::string, std::string>>& metadata()
      const noexcept {
    return metadata_;
  }
  [[nodiscard]] const std::string& error() const noexcept { return error_; }

 private:
  bool open_ = false;
  std::string profile_;
  std::string error_;
  std::vector<ChannelInfo> channels_;
  std::vector<Message> messages_;
  std::vector<Attachment> attachments_;
  bool in_summary_ = false;
  std::map<std::string, std::map<std::string, std::string>> metadata_;
};

}  // namespace srsp::record

#endif  // SRSP_RECORD_MCAP_HPP
