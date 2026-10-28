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
  return true;
}

bool McapWriter::close() {
  if (!open_) {
    return false;
  }

  // Records must appear as: magic, header, schema/channel/message, data end,
  // footer, magic. The header is built last but inserted first.
  std::vector<std::uint8_t> header_payload;
  put_string(header_payload, profile_);
  put_string(header_payload, "srsp-mcap/0.1");

  std::vector<std::uint8_t> data_end;
  put_u32(data_end, 0);  // data_section_crc: not computed

  std::vector<std::uint8_t> footer;
  put_u64(footer, 0);  // summary_start: no summary
  put_u64(footer, 0);  // summary_offset_start
  put_u32(footer, 0);  // summary_crc

  const auto encode = [](std::uint8_t opcode, const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> record;
    record.push_back(opcode);
    for (int i = 0; i < 8; ++i) {
      record.push_back(static_cast<std::uint8_t>((payload.size() >> (8 * i)) & 0xFFu));
    }
    record.insert(record.end(), payload.begin(), payload.end());
    return record;
  };

  std::vector<std::uint8_t> file;
  file.insert(file.end(), std::begin(kMagic), std::end(kMagic));
  const std::vector<std::uint8_t> header_record = encode(kOpHeader, header_payload);
  file.insert(file.end(), header_record.begin(), header_record.end());
  file.insert(file.end(), buffer_.begin(), buffer_.end());
  const std::vector<std::uint8_t> end_record = encode(kOpDataEnd, data_end);
  file.insert(file.end(), end_record.begin(), end_record.end());
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
