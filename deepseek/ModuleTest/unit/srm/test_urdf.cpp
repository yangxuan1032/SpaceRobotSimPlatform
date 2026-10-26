/**
 * @file test_urdf.cpp
 * @brief Unit tests for the URDF -> SRM importer.
 */
#include <srsp/model/urdf_importer.hpp>

#include <string>

#include <catch2/catch_test_macros.hpp>

namespace {

const char* kTwoLinkArm = R"(<?xml version="1.0"?>
<robot name="two_link">
  <link name="base">
    <inertial>
      <mass value="20.0"/>
      <origin xyz="0 0 0.1"/>
      <inertia ixx="1.0" iyy="1.0" izz="0.5" ixy="0" ixz="0" iyz="0"/>
    </inertial>
  </link>
  <link name="link1">
    <inertial>
      <mass value="5.0"/>
      <origin xyz="0 0 0.2"/>
      <inertia ixx="0.2" iyy="0.2" izz="0.05" ixy="0" ixz="0" iyz="0"/>
    </inertial>
    <visual>
      <geometry><mesh filename="meshes/link1.stl" scale="1 1 1"/></geometry>
    </visual>
  </link>
  <link name="link2">
    <inertial>
      <mass value="3.0"/>
      <inertia ixx="0.1" iyy="0.1" izz="0.02" ixy="0" ixz="0" iyz="0"/>
    </inertial>
  </link>
  <joint name="j1" type="revolute">
    <parent link="base"/>
    <child link="link1"/>
    <origin xyz="0 0 0.3" rpy="0 0 0"/>
    <axis xyz="0 0 1"/>
    <limit lower="-1.5" upper="1.5"/>
  </joint>
  <joint name="j2" type="continuous">
    <parent link="link1"/>
    <child link="link2"/>
    <origin xyz="0 0 0.4" rpy="0 1.2 0"/>
    <axis xyz="0 1 0"/>
  </joint>
</robot>)";

}  // namespace

TEST_CASE("urdf import builds the kinematic tree", "[urdf]") {
  const auto result = srsp::model::import_urdf(kTwoLinkArm);
  for (const auto& issue : result.issues) {
    INFO(issue.where << ": " << issue.message);
  }
  REQUIRE(result.ok());

  const auto& model = result.model;
  CHECK(model.name == "two_link");
  REQUIRE(model.bodies.size() == 3u);
  REQUIRE(model.joints.size() == 2u);

  // The base is the only body without an incoming joint.
  CHECK(model.bodies[0].id == "base");
  CHECK(model.bodies[0].parent_joint.empty());
  CHECK(model.bodies[1].parent_joint == "j1");
  CHECK(model.bodies[2].parent_joint == "j2");
}

TEST_CASE("urdf import keeps inertial and joint data", "[urdf]") {
  const auto result = srsp::model::import_urdf(kTwoLinkArm);
  REQUIRE(result.ok());

  const auto& base = result.model.bodies[0];
  CHECK(base.inertia.mass == 20.0);
  CHECK(base.inertia.com[2] == 0.1);
  CHECK(base.inertia.inertia[0] == 1.0);
  CHECK(base.inertia.inertia[8] == 0.5);

  const auto& link1 = result.model.bodies[1];
  REQUIRE(link1.geometry.size() == 1u);
  CHECK(link1.geometry[0].mesh == "meshes/link1.stl");

  const auto& j1 = result.model.joints[0];
  CHECK(j1.type == srsp::model::JointType::kRevolute);
  CHECK(j1.axis[2] == 1.0);
  CHECK(j1.lower == -1.5);
  CHECK(j1.upper == 1.5);
  CHECK(j1.origin[2] == 0.3);

  const auto& j2 = result.model.joints[1];
  CHECK(j2.type == srsp::model::JointType::kContinuous);
  CHECK(j2.origin_rpy[1] == 1.2);
}

TEST_CASE("urdf import rejects broken documents", "[urdf]") {
  const auto not_xml = srsp::model::import_urdf("<robot");
  CHECK_FALSE(not_xml.ok());

  const auto no_robot = srsp::model::import_urdf("<model/>");
  CHECK_FALSE(no_robot.ok());

  const auto bad_joint = srsp::model::import_urdf(R"(<robot name="x">
    <link name="a"/><link name="b"/>
    <joint name="j" type="floating"><parent link="a"/><child link="b"/></joint>
  </robot>)");
  REQUIRE_FALSE(bad_joint.ok());
  bool unsupported = false;
  for (const auto& issue : bad_joint.issues) {
    unsupported = unsupported || issue.message.find("unsupported") != std::string::npos;
  }
  CHECK(unsupported);
}

TEST_CASE("urdf import reports a missing child link", "[urdf]") {
  const auto result = srsp::model::import_urdf(R"(<robot name="x">
    <link name="a"/>
    <joint name="j" type="revolute"><parent link="a"/><child link="ghost"/>
      <axis xyz="0 0 1"/><limit lower="0" upper="1"/></joint>
  </robot>)");
  REQUIRE_FALSE(result.ok());
  bool missing = false;
  for (const auto& issue : result.issues) {
    missing = missing || issue.message.find("child body") != std::string::npos;
  }
  CHECK(missing);
}
