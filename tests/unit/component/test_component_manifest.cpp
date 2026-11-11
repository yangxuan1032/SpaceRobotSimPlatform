/**
 * @file test_component_manifest.cpp
 * @brief W1: component manifests, entity identity and parameter precedence.
 */
#include <srsp/model/component.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using nlohmann::json;
using srsp::model::ComponentIssue;
using srsp::model::ComponentKind;
using srsp::model::ComponentManifest;
using srsp::model::contributes_to_dynamics;
using srsp::model::EntityCatalog;
using srsp::model::format_issues;
using srsp::model::ParamResolver;
using srsp::model::ParamSource;
using srsp::model::parse_component_kind;
using srsp::model::validate_component_manifest;

namespace {

json camera_manifest() {
  return json::parse(R"({
    "schema": "srsp.component/1",
    "kind": "sensor.camera",
    "id": "org.srsp.sensors.camera",
    "version": "1.0.0",
    "capabilities": ["srsp.sensor/device@1"],
    "params": {
      "type": "object",
      "required": ["width", "height"],
      "properties": {
        "width": {"type": "integer", "default": 1280},
        "height": {"type": "integer", "default": 720},
        "fov_deg": {"type": "number", "default": 45.0}
      }
    },
    "mount": {"kind": "fixed", "default_mass_kg": 0.0},
    "visual": {"texture": "camera_albedo.png"},
    "checkpoint": true
  })");
}

}  // namespace

TEST_CASE("a valid manifest parses into typed fields", "[component]") {
  ComponentManifest manifest{};
  std::vector<ComponentIssue> issues;
  CHECK(validate_component_manifest(camera_manifest(), &manifest, &issues));
  CHECK(issues.empty());
  CHECK(manifest.kind == ComponentKind::kSensor);
  CHECK(manifest.id == "org.srsp.sensors.camera");
  CHECK(manifest.version == "1.0.0");
  CHECK(manifest.capabilities.size() == 1u);
  CHECK(manifest.checkpoint);
}

TEST_CASE("missing and malformed fields are reported by path", "[component]") {
  std::vector<ComponentIssue> issues;
  json missing = json::object();
  CHECK_FALSE(validate_component_manifest(missing, nullptr, &issues));
  CHECK(issues.size() >= 3u);
  const std::string text = format_issues(issues);
  CHECK(text.find("kind") != std::string::npos);
  CHECK(text.find("version") != std::string::npos);

  issues.clear();
  json bad = camera_manifest();
  bad["version"] = "1.0";
  bad["id"] = "Camera";
  bad["capabilities"] = json::array({"sensor/device"});
  CHECK_FALSE(validate_component_manifest(bad, nullptr, &issues));
  CHECK(format_issues(issues).find("MAJOR.MINOR.PATCH") != std::string::npos);
  CHECK(format_issues(issues).find("capability@major") != std::string::npos);
}

TEST_CASE("unknown kinds are rejected", "[component]") {
  json document = camera_manifest();
  document["kind"] = "sensor.magic";
  std::vector<ComponentIssue> issues;
  CHECK_FALSE(validate_component_manifest(document, nullptr, &issues));
  CHECK(format_issues(issues).find("unknown component kind") != std::string::npos);
  CHECK_FALSE(parse_component_kind("sensor.magic").has_value());
  CHECK(parse_component_kind("dynamics.body") == ComponentKind::kBody);
}

TEST_CASE("required parameters must be declared with a type", "[component]") {
  json document = camera_manifest();
  document["params"]["required"] = json::array({"width", "exposure_s"});
  std::vector<ComponentIssue> issues;
  CHECK_FALSE(validate_component_manifest(document, nullptr, &issues));
  CHECK(format_issues(issues).find("required property is not declared") != std::string::npos);

  issues.clear();
  json untyped = camera_manifest();
  untyped["params"]["properties"]["width"] = json::object();
  CHECK_FALSE(validate_component_manifest(untyped, nullptr, &issues));
  CHECK(format_issues(issues).find("needs a type") != std::string::npos);
}

TEST_CASE("visual resources may not hide inside dynamics parameters", "[component]") {
  json document = camera_manifest();
  document["kind"] = "dynamics.body";
  document["params"]["properties"]["texture"] = json{{"type", "string"}};
  std::vector<ComponentIssue> issues;
  CHECK_FALSE(validate_component_manifest(document, nullptr, &issues));
  CHECK(format_issues(issues).find("top-level visual") != std::string::npos);
}

TEST_CASE("mount kind and mass are validated", "[component]") {
  json document = camera_manifest();
  document["mount"] = json{{"kind", "glued"}, {"default_mass_kg", -1.0}};
  std::vector<ComponentIssue> issues;
  CHECK_FALSE(validate_component_manifest(document, nullptr, &issues));
  const std::string text = format_issues(issues);
  CHECK(text.find("fixed, jointed or logical") != std::string::npos);
  CHECK(text.find("must not be negative") != std::string::npos);
}

TEST_CASE("parameter resolution prefers CLI over project, model and default", "[component]") {
  ParamResolver resolver;
  resolver.set_default("fov_deg", "45");
  resolver.set_model("fov_deg", "50");
  resolver.set_project("fov_deg", "55");
  resolver.set_cli("fov_deg", "60");

  const auto value = resolver.resolve("fov_deg");
  REQUIRE(value.has_value());
  CHECK(value->value == "60");
  CHECK(value->source == ParamSource::kCli);

  CHECK_FALSE(resolver.resolve("missing").has_value());
  CHECK(resolver.resolved().size() == 1u);
}

TEST_CASE("entity ids are unique and conflicts name both sources", "[component]") {
  EntityCatalog catalog;
  std::string error;
  CHECK(catalog.add("entity/satellite", "satellite.srm.json", &error));
  CHECK(catalog.size() == 1u);
  CHECK(catalog.contains("entity/satellite"));

  CHECK_FALSE(catalog.add("entity/satellite", "override.json", &error));
  CHECK(error.find("satellite.srm.json") != std::string::npos);
  CHECK(error.find("override.json") != std::string::npos);
  CHECK(catalog.size() == 1u);
}

TEST_CASE("only mass-carrying kinds contribute to dynamics", "[component]") {
  CHECK(contributes_to_dynamics(ComponentKind::kBody));
  CHECK(contributes_to_dynamics(ComponentKind::kJoint));
  CHECK(contributes_to_dynamics(ComponentKind::kActuator));
  CHECK(contributes_to_dynamics(ComponentKind::kPropulsion));
  CHECK_FALSE(contributes_to_dynamics(ComponentKind::kSensor));
  CHECK_FALSE(contributes_to_dynamics(ComponentKind::kCelestial));
  CHECK_FALSE(contributes_to_dynamics(ComponentKind::kController));
  CHECK_FALSE(contributes_to_dynamics(ComponentKind::kRenderer));
}
