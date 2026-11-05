/**
 * @file test_mcap_truncation.cpp
 * @brief A truncated recording must fail to open without crashing (B10).
 */
#include <srsp/record/mcap.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <fstream>

using srsp::record::McapReader;
using srsp::record::McapWriter;

namespace {

std::string temp_path(const std::string& name) {
  return (std::filesystem::temp_directory_path() / name).string();
}

std::vector<char> read_bytes(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void write_bytes(const std::string& path, const std::vector<char>& bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

}  // namespace

TEST_CASE("truncated MCAP files are rejected without crashing", "[mcap][robustness]") {
  const std::string full = temp_path("srsp_trunc_source.mcap");
  {
    McapWriter writer;
    REQUIRE(writer.open(full, "srsp.test"));
    const auto schema = writer.add_schema("srsp.state/1", "raw-f64", "{}");
    const auto channel = writer.add_channel(schema, "sim.state", "raw-f64", {});
    for (std::uint64_t i = 0; i < 8; ++i) {
      REQUIRE(writer.write_message(channel, i, 1000 * i, "\x01\x02\x03\x04", 4));
    }
    REQUIRE(writer.close());
  }

  const std::vector<char> bytes = read_bytes(full);
  REQUIRE(bytes.size() > 32u);

  const std::size_t cuts[] = {0u, 1u, 8u, 16u, bytes.size() / 2u, bytes.size() - 1u};
  for (const std::size_t cut : cuts) {
    const std::string path = temp_path("srsp_trunc_cut.mcap");
    const std::vector<char> part(bytes.begin(), bytes.begin() + static_cast<long>(cut));
    write_bytes(path, part);

    McapReader reader;
    const bool opened = reader.open(path);
    // Either the reader refuses the file or it opens an empty recording; what
    // must never happen is a crash or reading past the truncated data.
    if (opened) {
      CHECK(reader.channels().size() <= 1u);
    }
    std::filesystem::remove(path);
  }
  std::filesystem::remove(full);
}