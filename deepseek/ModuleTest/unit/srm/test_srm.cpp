/**
 * @file test_srm.cpp
 * @brief Unit tests for the SRM 1.0 model layer.
 *
 * The example model directory is provided by ctest through SRSP_TEST_MODELS.
 */
#include <srsp/model/srm.hpp>

#include <string>

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>

namespace {

std::string models_dir() {
  const char* value = std::getenv("SRSP_TEST_MODELS");
  return value != nullptr ? std::string(value) : std::string();
}

srsp::model::LoadResult load_example(const char* name) {
  return srsp::model::load_from_file(models_dir() + "/" + name);
}

void require_models_env() {
  if (models_dir().empty()) {
    SKIP("SRSP_TEST_MODELS is not set");
  }
}

}  // namespace

TEST_CASE("srm loads the service satellite", "[srm]") {
  require_models_env();
  const auto result = load_example("sat_service.srm.json");
  for (const auto& issue : result.issues) {
    INFO(issue.where << ": " << issue.message);
  }
  REQUIRE(result.ok());
  CHECK(result.model.name == "sat_service");
  CHECK(result.model.bodies.size() == 3u);
  CHECK(result.model.joints.size() == 2u);
  CHECK(result.model.actuators.size() == 5u);
  CHECK(result.model.sensors.size() == 3u);
}

TEST_CASE("srm loads the six-joint arm chain", "[srm]") {
  require_models_env();
  const auto result = load_example("arm6.srm.json");
  REQUIRE(result.ok());
  CHECK(result.model.bodies.size() == 7u);
  CHECK(result.model.joints.size() == 6u);
  CHECK(result.model.joints.front().type == srsp::model::JointType::kRevolute);
}

TEST_CASE("srm loads the passive target", "[srm]") {
  require_models_env();
  const auto result = load_example("sat_target.srm.json");
  REQUIRE(result.ok());
  CHECK(result.model.bodies.size() == 2u);
  CHECK(result.model.joints.size() == 1u);
  CHECK(result.model.joints.front().type == srsp::model::JointType::kFixed);
}

TEST_CASE("srm rejects a malformed document", "[srm]") {
  const auto bad_json = srsp::model::load_from_json("{ not json");
  CHECK_FALSE(bad_json.ok());
  CHECK(bad_json.issues.front().where == "<document>");

  const auto wrong_schema = srsp::model::load_from_json(R"({"schema":"srsp.model/2"})");
  CHECK_FALSE(wrong_schema.ok());
  CHECK(wrong_schema.issues.front().where == "schema");
}

TEST_CASE("srm reports duplicate ids and unknown references", "[srm]") {
  const char* document = R"({
    "schema": "srsp.model/1",
    "name": "broken",
    "bodies": [
      {"id": "a", "inertia": {"mass": 1.0}},
      {"id": "a", "inertia": {"mass": 1.0}}
    ],
    "joints": [
      {"id": "j", "parent": "a", "child": "missing", "type": "revolute",
       "axis": [0, 0, 1], "lower": 0.0, "upper": 1.0}
    ]
  })";
  const auto result = srsp::model::load_from_json(document);
  REQUIRE_FALSE(result.ok());
  bool duplicate = false;
  bool missing_child = false;
  for (const auto& issue : result.issues) {
    duplicate = duplicate || issue.message.find("duplicate") != std::string::npos;
    missing_child = missing_child || issue.message.find("child body") != std::string::npos;
  }
  CHECK(duplicate);
  CHECK(missing_child);
}

TEST_CASE("srm detects a kinematic cycle", "[srm]") {
  const char* document = R"({
    "schema": "srsp.model/1",
    "name": "cycle",
    "bodies": [
      {"id": "a", "inertia": {"mass": 1.0}},
      {"id": "b", "parent_joint": "j1", "inertia": {"mass": 1.0}}
    ],
    "joints": [
      {"id": "j1", "parent": "a", "child": "b", "type": "revolute",
       "axis": [0, 0, 1], "lower": 0.0, "upper": 1.0},
      {"id": "j2", "parent": "b", "child": "a", "type": "revolute",
       "axis": [0, 0, 1], "lower": 0.0, "upper": 1.0}
    ]
  })";
  const auto result = srsp::model::load_from_json(document);
  REQUIRE_FALSE(result.ok());
  bool cycle = false;
  for (const auto& issue : result.issues) {
    cycle = cycle || issue.message.find("cycle") != std::string::npos;
  }
  CHECK(cycle);
}

TEST_CASE("srm round-trips through JSON", "[srm]") {
  require_models_env();
  const auto first = load_example("sat_service.srm.json");
  REQUIRE(first.ok());

  const std::string encoded = srsp::model::to_json(first.model);
  const auto second = srsp::model::load_from_json(encoded);
  REQUIRE(second.ok());
  CHECK(second.model.bodies.size() == first.model.bodies.size());
  CHECK(second.model.joints.size() == first.model.joints.size());
  CHECK(second.model.actuators.size() == first.model.actuators.size());
  CHECK(second.model.sensors.size() == first.model.sensors.size());
  CHECK(second.model.bodies.front().inertia.mass == first.model.bodies.front().inertia.mass);
}
