/**
 * @file test_mcap.cpp
 * @brief Unit tests for the MCAP subset writer/reader and state hashing.
 */
#include <srsp/core/blob.hpp>
#include <srsp/record/blob_recorder.hpp>
#include <srsp/record/blob_ref_codec.hpp>
#include <srsp/record/mcap.hpp>
#include <srsp/record/selection.hpp>
#include <srsp/record/state_hash.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>

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
  if (std::getenv("SRSP_KEEP_MCAP") == nullptr) {
    std::filesystem::remove(path);
  }
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
  if (std::getenv("SRSP_KEEP_MCAP") == nullptr) {
    std::filesystem::remove(path);
  }
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
  if (std::getenv("SRSP_KEEP_MCAP") == nullptr) {
    std::filesystem::remove(path);
  }
}
TEST_CASE("mcap stores blob attachments with metadata", "[mcap]") {
  const std::string path = temp_path("srsp_mcap_blob.mcap");
  const std::vector<std::uint8_t> payload{0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03};
  {
    srsp::record::McapWriter writer;
    REQUIRE(writer.open(path, "srsp.test"));
    const auto schema = writer.add_schema("srsp.state/1", "raw-f64", "{}");
    const auto channel = writer.add_channel(schema, "sim.state", "raw-f64");
    REQUIRE(writer.write_message(channel, 1, 5000, payload.data(), payload.size()));
    REQUIRE(writer.add_attachment("blob/7", 5000, "application/octet-stream", payload.data(),
                                  payload.size()));
    REQUIRE(writer.add_metadata("blob/7", {{"blob_generation", "3"}, {"schema_hash", "abc"}}));
    REQUIRE(writer.close());
  }

  srsp::record::McapReader reader;
  REQUIRE(reader.open(path));
  REQUIRE(reader.attachments().size() == 1u);
  const auto& attachment = reader.attachments()[0];
  CHECK(attachment.name == "blob/7");
  CHECK(attachment.log_time_ns == 5000u);
  CHECK(attachment.data == payload);

  REQUIRE(reader.metadata().count("blob/7") == 1u);
  CHECK(reader.metadata().at("blob/7").at("blob_generation") == "3");
  CHECK(reader.metadata().at("blob/7").at("schema_hash") == "abc");
  if (std::getenv("SRSP_KEEP_MCAP") == nullptr) {
    std::filesystem::remove(path);
  }
}
TEST_CASE("blob recorder writes and releases", "[mcap][blob]") {
  const std::string path = temp_path("srsp_blob_recorder.mcap");
  srsp::core::BlobStore store;
  const auto ref = store.store({1, 2, 3, 4});

  {
    srsp::record::McapWriter writer;
    REQUIRE(writer.open(path, "srsp.test"));
    srsp::record::BlobRecorder recorder(writer, store);
    REQUIRE(recorder.write(ref, 1000));
    CHECK(recorder.count() == 1u);
    CHECK(store.size() == 0u);  // reference released after the attachment landed
    REQUIRE(writer.close());
  }

  srsp::record::McapReader reader;
  REQUIRE(reader.open(path));
  REQUIRE(reader.attachments().size() == 1u);
  CHECK(reader.attachments()[0].name == "blob/" + std::to_string(ref.id));
  CHECK(reader.metadata().at(reader.attachments()[0].name).at("blob_generation") ==
        std::to_string(ref.generation));
  if (std::getenv("SRSP_KEEP_MCAP") == nullptr) {
    std::filesystem::remove(path);
  }
}

TEST_CASE("blob recorder refuses oversized payloads", "[mcap][blob]") {
  const std::string path = temp_path("srsp_blob_limits.mcap");
  srsp::core::BlobStore store;
  const auto ref = store.store(std::vector<std::uint8_t>(8, 0x5A));

  srsp::record::McapWriter writer;
  REQUIRE(writer.open(path, "srsp.test"));
  srsp::record::BlobLimits limits;
  limits.max_blob_bytes = 4;
  srsp::record::BlobRecorder recorder(writer, store, limits);
  CHECK_FALSE(recorder.write(ref, 1));
  CHECK(recorder.error().find("max_blob_bytes") != std::string::npos);
  CHECK(store.size() == 1u);  // nothing was released on failure
  REQUIRE(writer.close());
  if (std::getenv("SRSP_KEEP_MCAP") == nullptr) {
    std::filesystem::remove(path);
  }
}

TEST_CASE("blob recorder rejects stale references", "[mcap][blob]") {
  const std::string path = temp_path("srsp_blob_stale.mcap");
  srsp::core::BlobStore store;
  const auto ref = store.store({9, 9});
  store.release(ref);

  srsp::record::McapWriter writer;
  REQUIRE(writer.open(path, "srsp.test"));
  srsp::record::BlobRecorder recorder(writer, store);
  CHECK_FALSE(recorder.write(ref, 1));
  CHECK(recorder.error().find("stale") != std::string::npos);
  REQUIRE(writer.close());
  if (std::getenv("SRSP_KEEP_MCAP") == nullptr) {
    std::filesystem::remove(path);
  }
}
TEST_CASE("blobref wire format round-trips", "[mcap][blob]") {
  const srsp::core::BlobRef ref{42, 7, 1024, 0};
  std::uint8_t wire[srsp::record::kBlobRefWireSize]{};
  srsp::record::encode_blob_ref(ref, wire);
  CHECK(wire[0] == 42);
  CHECK(wire[8] == 7);

  srsp::core::BlobRef decoded{};
  REQUIRE(srsp::record::decode_blob_ref(wire, sizeof(wire), &decoded));
  CHECK(decoded.id == ref.id);
  CHECK(decoded.generation == ref.generation);
  CHECK(decoded.size_bytes == ref.size_bytes);
  CHECK(decoded.flags == ref.flags);
}

TEST_CASE("blobref decoder rejects short and invalid input", "[mcap][blob]") {
  std::uint8_t wire[srsp::record::kBlobRefWireSize]{};
  srsp::core::BlobRef out{};
  CHECK_FALSE(srsp::record::decode_blob_ref(wire, srsp::record::kBlobRefWireSize - 1, &out));

  std::uint8_t zeros[srsp::record::kBlobRefWireSize]{};
  CHECK_FALSE(srsp::record::decode_blob_ref(zeros, sizeof(zeros), &out));
}
TEST_CASE("channel metadata records the selection tags", "[mcap][selection]") {
  const std::string path = temp_path("srsp_selection.mcap");
  {
    srsp::record::McapWriter writer;
    REQUIRE(writer.open(path, "srsp.test"));
    const auto schema = writer.add_schema("srsp.state/1", "raw-f64", "{}");
    const auto metadata = srsp::record::selection_metadata(
        srsp::record::IntegratorTag::kSemiImplicit, srsp::record::SolverTag::kComposite,
        srsp::record::EncodingTag::kRawF64);
    const auto channel = writer.add_channel(schema, "sim.state", "raw-f64", metadata);
    REQUIRE(writer.write_message(channel, 1, 1000, "abcd", 4));
    REQUIRE(writer.close());
  }

  srsp::record::McapReader reader;
  REQUIRE(reader.open(path));
  REQUIRE(reader.channels().size() == 1u);
  const auto& metadata = reader.channels()[0].metadata;
  CHECK(metadata.at("integrator") == "semi_implicit");
  CHECK(metadata.at("solver") == "composite");
  CHECK(metadata.at("encoding") == "raw-f64");
  std::filesystem::remove(path);
}
