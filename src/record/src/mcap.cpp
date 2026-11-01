/**
 * @file mcap.cpp
 * @brief Minimal MCAP writer and reader for the M1 recording subset.
 */
#include <srsp/record/mcap.hpp>

#include <cstring>

#include <cstdio>
#include <fstream>
#include <iterator>

namespace srsp::record {
namespace {

constexpr std::uint8_t kMagic[8] = {0x89, 'M', 'C', 'A', 'P', '0', '\r', '\n'};
constexpr std::uint8_t kOpHeader = 0x01;
constexpr std::uint8_t kOpFooter = 0x02;
constexpr std::uint8_t kOpSchema = 0x03;
constexpr std::uint8_t kOpChannel = 0x04;
constexpr std::uint8_t kOpMessage = 0x05;
constexpr std::uint8_t kOpAttachment = 0x09;
constexpr std::uint8_t kOpMetadata = 0x0C;
constexpr std::uint8_t kOpDataEnd = 0x0F;

void put_u8(std::vector<std::uint8_t>& out, std::uint8_t value) {
  out.push_back(value);
}

void put_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFu));
  }
}

void put_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
  for (int i = 0; i < 8; ++i) {
    out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFu));
  }
}

void put_string(std::vector<std::uint8_t>& out, const std::string& text) {
  put_u32(out, static_cast<std::uint32_t>(text.size()));
  out.insert(out.end(), text.begin(), text.end());
}

std::uint32_t read_u32(const std::uint8_t* data) {
  return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) |
         (static_cast<std::uint32_t>(data[2]) << 16) | (static_cast<std::uint32_t>(data[3]) << 24);
}

std::uint64_t read_u64(const std::uint8_t* data) {
  std::uint64_t value = 0;
  for (int i = 7; i >= 0; --i) {
    value = (value << 8) | data[i];
  }
  return value;
}

std::uint16_t read_u16(const std::uint8_t* data) {
  return static_cast<std::uint16_t>(data[0] | (data[1] << 8));
}

}  // namespace

bool McapWriter::open(const std::string& path, const std::string& profile) {
  path_ = path;
  profile_ = profile;
  buffer_.clear();
  channels_.clear();
  messages_ = 0;
  error_ = false;
  open_ = true;
  return true;
}

bool McapWriter::write_record(std::uint8_t opcode, const std::vector<std::uint8_t>& payload) {
  if (!open_ || error_) {
    return false;
  }
  put_u8(buffer_, opcode);
  put_u64(buffer_, payload.size());
  buffer_.insert(buffer_.end(), payload.begin(), payload.end());
  return true;
}

std::uint32_t McapWriter::add_schema(const std::string& name, const std::string& encoding,
                                     const std::string& data) {
  const std::uint32_t schema_id = static_cast<std::uint32_t>(channels_.size()) + 1;
  std::vector<std::uint8_t> payload;
  put_u16(payload, schema_id);
  put_string(payload, name);
  put_string(payload, encoding);
  put_u32(payload, static_cast<std::uint32_t>(data.size()));
  payload.insert(payload.end(), data.begin(), data.end());
  if (!write_record(kOpSchema, payload)) {
    return 0;
  }
  ++schema_count_;
  schemas_.push_back(SchemaEntry{schema_id, name, encoding, data});
  // Schema ids are tracked through the channel table; store the pending id.
  pending_schema_id_ = schema_id;
  pending_schema_name_ = name;
  pending_schema_encoding_ = encoding;
  return schema_id;
}

std::uint32_t McapWriter::add_channel(std::uint32_t schema_id, const std::string& topic,
                                      const std::string& message_encoding,
                                      const std::map<std::string, std::string>& metadata) {
  const std::uint32_t channel_id = static_cast<std::uint32_t>(channels_.size()) + 1;
  std::vector<std::uint8_t> payload;
  put_u16(payload, channel_id);
  put_u16(payload, schema_id);
  put_string(payload, topic);
  put_string(payload, message_encoding);
  put_u32(payload, static_cast<std::uint32_t>(metadata.size()));
  for (const auto& [key, value] : metadata) {
    put_string(payload, key);
    put_string(payload, value);
  }
  if (!write_record(kOpChannel, payload)) {
    return 0;
  }
  ChannelInfo info;
  info.id = channel_id;
  info.schema_id = schema_id;
  info.topic = topic;
  info.message_encoding = message_encoding;
  info.metadata = metadata;
  info.schema_name = pending_schema_name_;
  info.schema_encoding = pending_schema_encoding_;
  channels_.push_back(std::move(info));
  return channel_id;
}

bool McapWriter::write_message(std::uint32_t channel_id, std::uint64_t sequence,
                               std::uint64_t log_time_ns, const void* data, std::size_t size) {
  std::vector<std::uint8_t> payload;
  put_u16(payload, static_cast<std::uint16_t>(channel_id));
  put_u32(payload, static_cast<std::uint32_t>(sequence));
  put_u64(payload, log_time_ns);
  put_u64(payload, log_time_ns);
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  payload.insert(payload.end(), bytes, bytes + size);
  if (!write_record(kOpMessage, payload)) {
    return false;
  }
  ++messages_;
  if (!has_time_) {
    first_time_ = log_time_ns;
    last_time_ = log_time_ns;
    has_time_ = true;
  } else {
    first_time_ = std::min(first_time_, log_time_ns);
    last_time_ = std::max(last_time_, log_time_ns);
  }
  return true;
}

bool McapWriter::add_attachment(const std::string& name, std::uint64_t log_time_ns,
                                const std::string& media_type, const void* data, std::size_t size) {
  if (!open_ || data == nullptr) {
    return false;
  }
  const std::uint64_t record_offset = buffer_.size();
  std::vector<std::uint8_t> payload;
  put_u64(payload, log_time_ns);  // log_time
  put_u64(payload, log_time_ns);  // create_time
  put_string(payload, name);
  put_string(payload, media_type);
  put_u64(payload, static_cast<std::uint64_t>(size));
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  payload.insert(payload.end(), bytes, bytes + size);
  put_u32(payload, 0);  // crc: not computed in the subset
  if (!write_record(kOpAttachment, payload)) {
    return false;
  }
  attachment_entries_.push_back(
      AttachmentEntry{record_offset, 9 + payload.size(), log_time_ns, size, name, media_type});
  return true;
}

bool McapWriter::add_metadata(const std::string& name,
                              const std::map<std::string, std::string>& entries) {
  if (!open_) {
    return false;
  }
  const std::uint64_t record_offset = buffer_.size();
  std::vector<std::uint8_t> payload;
  put_string(payload, name);
  put_u32(payload, static_cast<std::uint32_t>(entries.size()));
  for (const auto& [key, value] : entries) {
    put_string(payload, key);
    put_string(payload, value);
  }
  if (!write_record(kOpMetadata, payload)) {
    return false;
  }
  metadata_entries_.push_back(MetadataEntry{record_offset, 9 + payload.size(), name});
  return true;
}

bool McapWriter::close() {
  if (!open_) {
    return false;
  }

  std::vector<std::uint8_t> header_payload;
  put_string(header_payload, profile_);
  put_string(header_payload, "srsp-mcap/0.1");

  const auto encode = [](std::uint8_t opcode, const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> record;
    record.push_back(opcode);
    for (int i = 0; i < 8; ++i) {
      record.push_back(static_cast<std::uint8_t>((payload.size() >> (8 * i)) & 0xFFu));
    }
    record.insert(record.end(), payload.begin(), payload.end());
    return record;
  };

  // Absolute offsets: magic + header record + everything written so far.
  const std::uint64_t base = 8ull + 9ull + header_payload.size();
  std::vector<std::uint8_t> summary;
  std::uint64_t cursor = base + buffer_.size() + 13ull;  // past the DataEnd record

  struct Group {
    std::uint8_t opcode = 0;
    std::uint64_t start = 0;
    std::uint64_t length = 0;
  };
  std::vector<Group> groups;
  const auto append = [&](std::uint8_t opcode, const std::vector<std::uint8_t>& payload) {
    const std::vector<std::uint8_t> record = encode(opcode, payload);
    groups.push_back(Group{opcode, cursor, record.size()});
    summary.insert(summary.end(), record.begin(), record.end());
    cursor += record.size();
  };

  for (const auto& schema : schemas_) {
    std::vector<std::uint8_t> payload;
    put_u16(payload, static_cast<std::uint16_t>(schema.id));
    put_string(payload, schema.name);
    put_string(payload, schema.encoding);
    put_u32(payload, static_cast<std::uint32_t>(schema.data.size()));
    payload.insert(payload.end(), schema.data.begin(), schema.data.end());
    append(0x03u, payload);
  }
  for (const auto& channel : channels_) {
    std::vector<std::uint8_t> payload;
    put_u16(payload, static_cast<std::uint16_t>(channel.id));
    put_u16(payload, static_cast<std::uint16_t>(channel.schema_id));
    put_string(payload, channel.topic);
    put_string(payload, channel.message_encoding);
    put_u32(payload, static_cast<std::uint32_t>(channel.metadata.size()));
    for (const auto& [key, value] : channel.metadata) {
      put_string(payload, key);
      put_string(payload, value);
    }
    append(0x04u, payload);
  }

  std::vector<std::uint8_t> statistics;
  put_u64(statistics, messages_);
  put_u16(statistics, schema_count_);
  put_u32(statistics, static_cast<std::uint32_t>(channels_.size()));
  put_u32(statistics, static_cast<std::uint32_t>(attachment_entries_.size()));
  put_u32(statistics, static_cast<std::uint32_t>(metadata_entries_.size()));
  put_u32(statistics, 0);  // chunk_count
  put_u64(statistics, first_time_);
  put_u64(statistics, last_time_);
  put_u32(statistics, 0);     // channel_message_counts: empty map
  append(0x0Bu, statistics);  // Statistics

  std::vector<std::uint8_t> attachment_index;
  put_u32(attachment_index, static_cast<std::uint32_t>(attachment_entries_.size()));
  for (const auto& entry : attachment_entries_) {
    put_u64(attachment_index, base + entry.offset);
    put_u64(attachment_index, entry.length);
    put_u64(attachment_index, entry.log_time_ns);
    put_u64(attachment_index, entry.log_time_ns);  // create_time
    put_u64(attachment_index, entry.data_size);
    put_string(attachment_index, entry.name);
    put_string(attachment_index, entry.media_type);
  }
  append(0x0Au, attachment_index);  // AttachmentIndex

  std::vector<std::uint8_t> metadata_index;
  put_u32(metadata_index, static_cast<std::uint32_t>(metadata_entries_.size()));
  for (const auto& entry : metadata_entries_) {
    put_u64(metadata_index, base + entry.offset);
    put_u64(metadata_index, entry.length);
    put_string(metadata_index, entry.name);
  }
  append(0x0Du, metadata_index);  // MetadataIndex

  const std::uint64_t summary_start = base + buffer_.size() + 13ull;
  const std::uint64_t summary_offset_start = cursor;

  std::vector<std::uint8_t> summary_offsets;
  put_u32(summary_offsets, static_cast<std::uint32_t>(groups.size()));
  for (const auto& group : groups) {
    summary_offsets.push_back(group.opcode);
    put_u64(summary_offsets, group.start);
    put_u64(summary_offsets, group.length);
  }
  const std::vector<std::uint8_t> offset_record = encode(0x0Eu, summary_offsets);
  summary.insert(summary.end(), offset_record.begin(), offset_record.end());

  std::vector<std::uint8_t> footer;
  put_u64(footer, summary_start);
  put_u64(footer, summary_offset_start);
  put_u32(footer, 0);  // summary_crc: not computed

  std::vector<std::uint8_t> data_end;
  put_u32(data_end, 0);  // data_section_crc: not computed

  std::vector<std::uint8_t> file;
  file.insert(file.end(), std::begin(kMagic), std::end(kMagic));
  const std::vector<std::uint8_t> header_record = encode(kOpHeader, header_payload);
  file.insert(file.end(), header_record.begin(), header_record.end());
  file.insert(file.end(), buffer_.begin(), buffer_.end());
  const std::vector<std::uint8_t> end_record = encode(kOpDataEnd, data_end);
  file.insert(file.end(), end_record.begin(), end_record.end());
  file.insert(file.end(), summary.begin(), summary.end());
  const std::vector<std::uint8_t> footer_record = encode(kOpFooter, footer);
  file.insert(file.end(), footer_record.begin(), footer_record.end());
  file.insert(file.end(), std::begin(kMagic), std::end(kMagic));

  std::ofstream output(path_, std::ios::binary | std::ios::trunc);
  if (!output) {
    open_ = false;
    return false;
  }
  output.write(reinterpret_cast<const char*>(file.data()),
               static_cast<std::streamsize>(file.size()));
  output.close();
  open_ = false;
  return static_cast<bool>(output);
}

bool McapReader::open(const std::string& path) {
  open_ = false;
  error_.clear();
  channels_.clear();
  messages_.clear();

  std::ifstream input(path, std::ios::binary);
  if (!input) {
    error_ = "cannot open file";
    return false;
  }
  std::vector<std::uint8_t> file((std::istreambuf_iterator<char>(input)),
                                 std::istreambuf_iterator<char>());
  if (file.size() < 16 || std::memcmp(file.data(), kMagic, 8) != 0 ||
      std::memcmp(file.data() + file.size() - 8, kMagic, 8) != 0) {
    error_ = "bad MCAP magic";
    return false;
  }

  std::size_t offset = 8;
  while (offset + 9 <= file.size() - 8) {
    const std::uint8_t opcode = file[offset];
    const std::uint64_t length = read_u64(file.data() + offset + 1);
    offset += 9;
    if (offset + length > file.size()) {
      error_ = "truncated record";
      return false;
    }
    const std::uint8_t* payload = file.data() + offset;
    if (opcode == kOpDataEnd) {
      in_summary_ = true;
    }
    // The summary repeats schema/channel records for indexing; skip them so the
    // data view is not duplicated.
    if (in_summary_ && (opcode == kOpSchema || opcode == kOpChannel || opcode == kOpMessage)) {
      offset += length;
      continue;
    }
    if (opcode == kOpHeader) {
      std::size_t cursor = 0;
      const std::uint32_t profile_size = read_u32(payload + cursor);
      cursor += 4;
      profile_.assign(reinterpret_cast<const char*>(payload + cursor), profile_size);
      cursor += profile_size;
    } else if (opcode == kOpChannel) {
      ChannelInfo info;
      info.id = read_u16(payload);
      info.schema_id = read_u16(payload + 2);
      std::size_t cursor = 4;
      const std::uint32_t topic_size = read_u32(payload + cursor);
      cursor += 4;
      info.topic.assign(reinterpret_cast<const char*>(payload + cursor), topic_size);
      cursor += topic_size;
      const std::uint32_t encoding_size = read_u32(payload + cursor);
      cursor += 4;
      info.message_encoding.assign(reinterpret_cast<const char*>(payload + cursor), encoding_size);
      cursor += encoding_size;
      const std::uint32_t meta_count = read_u32(payload + cursor);
      cursor += 4;
      for (std::uint32_t m = 0; m < meta_count; ++m) {
        const std::uint32_t key_size = read_u32(payload + cursor);
        cursor += 4;
        std::string key(reinterpret_cast<const char*>(payload + cursor), key_size);
        cursor += key_size;
        const std::uint32_t value_size = read_u32(payload + cursor);
        cursor += 4;
        std::string value(reinterpret_cast<const char*>(payload + cursor), value_size);
        cursor += value_size;
        info.metadata.emplace(std::move(key), std::move(value));
      }
      channels_.push_back(std::move(info));
    } else if (opcode == kOpAttachment) {
      Attachment attachment;
      attachment.log_time_ns = read_u64(payload);
      std::size_t cursor = 16;  // log_time + create_time
      const std::uint32_t name_size = read_u32(payload + cursor);
      cursor += 4;
      attachment.name.assign(reinterpret_cast<const char*>(payload + cursor), name_size);
      cursor += name_size;
      const std::uint32_t media_size = read_u32(payload + cursor);
      cursor += 4;
      attachment.media_type.assign(reinterpret_cast<const char*>(payload + cursor), media_size);
      cursor += media_size;
      const std::uint64_t data_size = read_u64(payload + cursor);
      cursor += 8;
      attachment.data.assign(payload + cursor, payload + cursor + data_size);
      attachments_.push_back(std::move(attachment));
    } else if (opcode == kOpMetadata) {
      std::size_t cursor = 0;
      const std::uint32_t name_size = read_u32(payload + cursor);
      cursor += 4;
      std::string name(reinterpret_cast<const char*>(payload + cursor), name_size);
      cursor += name_size;
      const std::uint32_t count = read_u32(payload + cursor);
      cursor += 4;
      std::map<std::string, std::string> entries;
      for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t key_size = read_u32(payload + cursor);
        cursor += 4;
        std::string key(reinterpret_cast<const char*>(payload + cursor), key_size);
        cursor += key_size;
        const std::uint32_t value_size = read_u32(payload + cursor);
        cursor += 4;
        std::string value(reinterpret_cast<const char*>(payload + cursor), value_size);
        cursor += value_size;
        entries.emplace(std::move(key), std::move(value));
      }
      metadata_.emplace(std::move(name), std::move(entries));
    } else if (opcode == kOpMessage) {
      Message message;
      message.channel_id = read_u16(payload);
      message.sequence = read_u32(payload + 2);
      message.log_time_ns = read_u64(payload + 6);
      const std::size_t header_size = 22;  // 2 + 4 + 8 + 8
      message.data.assign(payload + header_size, payload + length);
      messages_.push_back(std::move(message));
    }
    offset += length;
  }

  open_ = true;
  return true;
}

}  // namespace srsp::record
