/**
 * @file test_mcap.cpp
 * @brief Unit tests for the MCAP subset writer/reader and state hashing.
 */
#include <srsp/record/mcap.hpp>
#include <srsp/record/state_hash.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <cstdio>

namespace {

std::string temp_path(const char* name) {
  return (std::filesystem::temp_directory_path() / name).string();
}

}  // namespace

TEST_CASE("mcap round-trips messages", "[mcap]") {
  const std::string path = temp_path("srsp_mcap_roundtrip.mcap");
  {
    srsp::record::McapWriter writer;
    REQUIRE(writer.open(path, "srsp.test"));
    const std::uint32_t schema = writer.add_schema("srsp.state/1", "jsonschema", "{}");
    REQUIRE(schema == 1u);
    const std::uint32_t channel = writer.add_channel(schema, "sim.state", "json");
    REQUIRE(channel == 1u);

    const char first[] = "{\"step\":1}";
    const char second[] = "{\"step\":2}";
    REQUIRE(writer.write_message(channel, 1, 1000, first, sizeof(first) - 1));
    REQUIRE(writer.write_message(channel, 2, 2000, second, sizeof(second) - 1));
    CHECK(writer.message_count() == 2u);
    REQUIRE(writer.close());
  }

  srsp::record::McapReader reader;
  REQUIRE(reader.open(path));
  CHECK(reader.profile() == "srsp.test");
  REQUIRE(reader.channels().size() == 1u);
  CHECK(reader.channels()[0].topic == "sim.state");
  REQUIRE(reader.messages().size() == 2u);
  CHECK(reader.messages()[0].sequence == 1u);
  CHECK(reader.messages()[1].log_time_ns == 2000u);
  CHECK(std::string(reader.messages()[0].data.begin(), reader.messages()[0].data.end()) ==
        "{\"step\":1}");
  std::filesystem::remove(path);
}

TEST_CASE("mcap rejects a file with a broken magic", "[mcap]") {
  const std::string path = temp_path("srsp_mcap_bad.mcap");
  {
    std::FILE* file = std::fopen(path.c_str(), "wb");
    REQUIRE(file != nullptr);
    std::fputs("not an mcap file", file);
    std::fclose(file);
  }

  srsp::record::McapReader reader;
  CHECK_FALSE(reader.open(path));
  CHECK(reader.error().find("magic") != std::string::npos);
  std::filesystem::remove(path);
}

TEST_CASE("state hashing is stable and distinguishes states", "[mcap]") {
  const std::vector<double> a{1.0, 2.0, 3.0};
  const std::vector<double> b{1.0, 2.0, 3.0000001};

  const srsp::record::StateHash first = srsp::record::hash_state(a);
  const srsp::record::StateHash second = srsp::record::hash_state(a);
  const srsp::record::StateHash different = srsp::record::hash_state(b);

  CHECK(first == second);
  CHECK_FALSE(first == different);
  CHECK(first.to_hex().size() == 32u);
}
TEST_CASE("mcap preserves channel metadata", "[mcap]") {
  const std::string path = temp_path("srsp_mcap_metadata.mcap");
  {
    srsp::record::McapWriter writer;
    REQUIRE(writer.open(path, "srsp.test"));
    const auto schema = writer.add_schema("srsp.state/1", "raw-f64", "{}");
    const std::map<std::string, std::string> metadata{{"schema_hash", "0123456789abcdef"},
                                                      {"abi", "1"}};
    const auto channel = writer.add_channel(schema, "sim.state", "raw-f64", metadata);
    REQUIRE(channel == 1u);
    const char payload[] = "abcd";
    REQUIRE(writer.write_message(channel, 1, 1000, payload, 4));
    REQUIRE(writer.close());
  }

  srsp::record::McapReader reader;
  REQUIRE(reader.open(path));
  REQUIRE(reader.channels().size() == 1u);
  const auto& metadata = reader.channels()[0].metadata;
  CHECK(metadata.size() == 2u);
  CHECK(metadata.at("schema_hash") == "0123456789abcdef");
  CHECK(metadata.at("abi") == "1");
  std::filesystem::remove(path);
}
