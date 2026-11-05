/**
 * @file test_importer_robustness.cpp
 * @brief Malformed input must be reported, never crash (B10).
 *
 * These are the cheapest robustness checks: every importer sees empty, garbage,
 * truncated and type-mismatched input and has to return a non-ok result.
 */
#include <srsp/model/mjcf_importer.hpp>
#include <srsp/model/srm.hpp>
#include <srsp/model/urdf_importer.hpp>

#include <string>

#include <catch2/catch_test_macros.hpp>

using srsp::model::import_mjcf;
using srsp::model::import_urdf;
using srsp::model::load_from_json;

TEST_CASE("SRM rejects empty, garbage and truncated input", "[srm][robustness]") {
  const char* inputs[] = {
      "", "not json at all", "{\"schema\":\"srsp.model/1\",\"bodies\":[", "[]", "{}",
  };
  for (const char* text : inputs) {
    INFO("input: " << text);
    const auto result = load_from_json(text);
    CHECK_FALSE(result.ok());
  }
}

TEST_CASE("SRM rejects type mismatches and invalid values", "[srm][robustness]") {
  const char* cases[] = {
      // mass as string
      R"({"schema":"srsp.model/1","bodies":[{"id":"b","inertia":{"mass":"heavy","com":[0,0,0],"tensor":[1,0,0,0,1,0,0,0,1]}}]})",
      // negative mass
      R"({"schema":"srsp.model/1","bodies":[{"id":"b","inertia":{"mass":-5,"com":[0,0,0],"tensor":[1,0,0,0,1,0,0,0,1]}}]})",
      // unknown joint type
      R"({"schema":"srsp.model/1","bodies":[{"id":"a","inertia":{"mass":1,"com":[0,0,0],"tensor":[1,0,0,0,1,0,0,0,1]}},{"id":"b","parent_joint":"j","inertia":{"mass":1,"com":[0,0,0],"tensor":[1,0,0,0,1,0,0,0,1]}}],"joints":[{"id":"j","parent":"a","child":"b","type":"warp","axis":[0,0,1]}]})",
      // missing required body id
      R"({"schema":"srsp.model/1","bodies":[{"inertia":{"mass":1,"com":[0,0,0],"tensor":[1,0,0,0,1,0,0,0,1]}}]})",
  };
  for (const char* text : cases) {
    const auto result = load_from_json(text);
    CHECK_FALSE(result.ok());
  }
}

TEST_CASE("URDF and MJCF reject malformed XML", "[importers][robustness]") {
  const char* inputs[] = {
      "",
      "garbage",
      "<robot><link name=\"a\"></robot>",
      "<mujoco><worldbody><body>",
  };
  for (const char* text : inputs) {
    CHECK_FALSE(import_urdf(text).ok());
    CHECK_FALSE(import_mjcf(text).ok());
  }
}