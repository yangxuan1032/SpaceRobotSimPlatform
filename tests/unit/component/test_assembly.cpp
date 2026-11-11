/**
 * @file test_assembly.cpp
 * @brief W2: mount graph validation and assembly compilation.
 */
#include <srsp/model/assembly.hpp>

#include <catch2/catch_test_macros.hpp>

using srsp::model::AssemblyResult;
using srsp::model::AssemblySpec;
using srsp::model::build_assembly;
using srsp::model::ComponentFragment;
using srsp::model::instance_prefix;
using srsp::model::Joint;
using srsp::model::JointType;
using srsp::model::MountKind;
using srsp::model::MountSpec;

namespace {

ComponentFragment make_base() {
  ComponentFragment base;
  base.id = "entity/satellite";
  base.mass_kg = 500.0;
  return base;
}

ComponentFragment make_arm(const std::string& host, MountKind kind, double mass) {
  ComponentFragment arm;
  arm.id = "entity/arm";
  arm.mass_kg = mass;
  for (int i = 0; i < 6; ++i) {
    Joint joint;
    joint.id = "j" + std::to_string(i);
    joint.parent_body = i == 0 ? "base" : "link" + std::to_string(i - 1);
    joint.child_body = "link" + std::to_string(i);
    joint.type = JointType::kRevolute;
    arm.joints.push_back(joint);
  }
  MountSpec mount;
  mount.component_id = arm.id;
  mount.host = host;
  mount.frame = "body";
  mount.kind = kind;
  mount.mass_kg = mass;
  arm.mounts.push_back(mount);
  return arm;
}

ComponentFragment make_camera(const std::string& host, double mass) {
  ComponentFragment camera;
  camera.id = "entity/camera";
  camera.mass_kg = mass;
  MountSpec mount;
  mount.component_id = camera.id;
  mount.host = host;
  mount.frame = "body";
  mount.kind = MountKind::kFixed;
  mount.mass_kg = mass;
  camera.mounts.push_back(mount);
  return camera;
}

}  // namespace

TEST_CASE("instance prefixes keep ids unique per component", "[assembly]") {
  CHECK(instance_prefix("entity/satellite") == "satellite/");
  CHECK(instance_prefix("arm") == "arm/");
}

TEST_CASE("a base, an arm and a fixed camera merge into one model", "[assembly]") {
  // The camera lives on the arm and carries mass, so it becomes a child body;
  // the arm itself is mounted fixed on the satellite.
  ComponentFragment arm = make_arm("entity/satellite", MountKind::kFixed, 20.0);
  ComponentFragment camera = make_camera(arm.id, 0.5);
  arm.mounts.push_back(camera.mounts.front());

  AssemblySpec spec;
  spec.id = "satellite+arm+camera";
  spec.base = make_base();
  spec.parts = {arm, camera};

  const AssemblyResult result = build_assembly(spec);
  REQUIRE(result.ok);
  CHECK(result.errors.empty());
  CHECK(result.total_mass_kg == 520.5);
  // 6 arm joints + 1 fixed arm mount + 1 fixed camera mount.
  CHECK(result.model.joints.size() == 8u);
  // Every body is namespaced by its own instance, and the canonical ids appear.
  bool has_arm_body = false;
  bool has_camera_body = false;
  for (const auto& body : result.model.bodies) {
    CHECK(body.id.find('/') != std::string::npos);
    has_arm_body = has_arm_body || body.id == "arm/body";
    has_camera_body = has_camera_body || body.id == "camera/body";
  }
  CHECK(has_arm_body);
  CHECK(has_camera_body);
  CHECK(result.model.actuators.empty());
}

TEST_CASE("a zero-mass or logical component leaves nv unchanged", "[assembly]") {
  ComponentFragment camera = make_camera("entity/satellite", 0.0);
  AssemblySpec spec;
  spec.id = "satellite+camera";
  spec.base = make_base();
  spec.parts = {camera};

  const AssemblyResult result = build_assembly(spec);
  REQUIRE(result.ok);
  CHECK(result.model.joints.empty());
  CHECK(result.model.bodies.size() == 1u);
  CHECK(result.total_mass_kg == 500.0);
}

TEST_CASE("a missing host is rejected with its name", "[assembly]") {
  ComponentFragment camera = make_camera("entity/ghost", 1.0);
  AssemblySpec spec;
  spec.id = "bad";
  spec.base = make_base();
  spec.parts = {camera};

  const AssemblyResult result = build_assembly(spec);
  CHECK_FALSE(result.ok);
  REQUIRE_FALSE(result.errors.empty());
  CHECK(result.errors.front().find("entity/ghost") != std::string::npos);
}

TEST_CASE("a mount cycle is rejected", "[assembly]") {
  ComponentFragment base = make_base();
  ComponentFragment arm = make_arm("entity/camera", MountKind::kFixed, 1.0);
  ComponentFragment camera = make_camera("entity/arm", 1.0);

  AssemblySpec spec;
  spec.id = "cycle";
  spec.base = base;
  spec.parts = {arm, camera};

  const AssemblyResult result = build_assembly(spec);
  CHECK_FALSE(result.ok);
  bool saw_cycle = false;
  for (const auto& error : result.errors) {
    saw_cycle = saw_cycle || error.find("cycle") != std::string::npos;
  }
  CHECK(saw_cycle);
}

TEST_CASE("duplicate instance ids are rejected", "[assembly]") {
  ComponentFragment first = make_camera("entity/satellite", 1.0);
  ComponentFragment second = make_camera("entity/satellite", 1.0);
  second.id = first.id;

  AssemblySpec spec;
  spec.id = "dupe";
  spec.base = make_base();
  spec.parts = {first};
  spec.parts.push_back(second);

  const AssemblyResult result = build_assembly(spec);
  CHECK_FALSE(result.ok);
  bool saw_duplicate = false;
  for (const auto& error : result.errors) {
    saw_duplicate = saw_duplicate || error.find("duplicate") != std::string::npos;
  }
  CHECK(saw_duplicate);
}

TEST_CASE("negative mount mass is rejected", "[assembly]") {
  ComponentFragment camera = make_camera("entity/satellite", -1.0);
  AssemblySpec spec;
  spec.id = "negative";
  spec.base = make_base();
  spec.parts = {camera};

  const AssemblyResult result = build_assembly(spec);
  CHECK_FALSE(result.ok);
  bool saw_mass = false;
  for (const auto& error : result.errors) {
    saw_mass = saw_mass || error.find("negative") != std::string::npos;
  }
  CHECK(saw_mass);
}
