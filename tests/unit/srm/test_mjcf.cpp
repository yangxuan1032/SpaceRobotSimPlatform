/**
 * @file test_mjcf.cpp
 * @brief Unit tests for the MJCF subset importer.
 */
#include <srsp/model/mjcf_importer.hpp>

#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>

namespace {

const char* kTwoLink = R"(<mujoco model="mjcf_arm">
  <worldbody>
    <body name="base" pos="0 0 0">
      <freejoint name="floating"/>
      <inertial pos="0 0 0" mass="10" diaginertia="0.5 0.6 0.7"/>
      <body name="link1" pos="0 0 0.3" quat="1 0 0 0">
        <joint name="j1" type="hinge" axis="0 0 1" range="-1.5 1.5"/>
        <inertial pos="0 0 0.15" mass="2" diaginertia="0.1 0.1 0.02"/>
        <body name="link2" pos="0 0 0.3">
          <joint name="j2" type="slide" axis="1 0 0" range="-0.2 0.2"/>
          <inertial pos="0 0 0.1" mass="1" diaginertia="0.05 0.05 0.01"/>
        </body>
      </body>
    </body>
  </worldbody>
</mujoco>)";

}  // namespace

TEST_CASE("mjcf import builds the tree", "[mjcf]") {
  const auto result = srsp::model::import_mjcf(kTwoLink);
  for (const auto& issue : result.issues) {
    INFO(issue.where << ": " << issue.message);
  }
  REQUIRE(result.ok());

  CHECK(result.model.name == "mjcf_arm");
  REQUIRE(result.model.bodies.size() == 3u);
  REQUIRE(result.model.joints.size() == 2u);

  CHECK(result.model.bodies[0].id == "base");
  CHECK(result.model.bodies[1].parent_joint == "j1");
  CHECK(result.model.bodies[2].parent_joint == "j2");
}

TEST_CASE("mjcf import keeps inertial and joint data", "[mjcf]") {
  const auto result = srsp::model::import_mjcf(kTwoLink);
  REQUIRE(result.ok());

  CHECK(result.model.bodies[0].inertia.mass == 10.0);
  CHECK(result.model.bodies[0].inertia.inertia[0] == 0.5);
  CHECK(result.model.bodies[0].inertia.inertia[8] == 0.7);

  const auto& j1 = result.model.joints[0];
  CHECK(j1.type == srsp::model::JointType::kRevolute);
  CHECK(j1.axis[2] == 1.0);
  CHECK(j1.lower == -1.5);
  CHECK(j1.upper == 1.5);
  CHECK(j1.origin[2] == 0.3);

  const auto& j2 = result.model.joints[1];
  CHECK(j2.type == srsp::model::JointType::kPrismatic);
  CHECK(j2.axis[0] == 1.0);
}

TEST_CASE("mjcf importer rejects unsupported constructs", "[mjcf]") {
  const auto not_xml = srsp::model::import_mjcf("<mujoco");
  CHECK_FALSE(not_xml.ok());

  const auto no_worldbody = srsp::model::import_mjcf("<mujoco/>");
  CHECK_FALSE(no_worldbody.ok());

  const auto ball = srsp::model::import_mjcf(R"(<mujoco model="x"><worldbody>
    <body name="root"><freejoint/>
      <body name="child" pos="1 0 0">
        <joint name="ball" type="ball"/>
        <inertial mass="1" diaginertia="1 1 1"/>
      </body>
    </body>
  </worldbody></mujoco>)");
  for (const auto& issue : ball.issues) {
    std::fprintf(stderr, "ISSUE %s: %s\n", issue.where.c_str(), issue.message.c_str());
  }
  bool unsupported = false;
  for (const auto& issue : ball.issues) {
    unsupported = unsupported || issue.message.find("unsupported") != std::string::npos;
  }
  CHECK(unsupported);
}

TEST_CASE("mjcf quaternion is converted to rpy", "[mjcf]") {
  // 90 deg about z: quat = (cos45, 0, 0, sin45)
  const auto result = srsp::model::import_mjcf(R"(<mujoco model="q"><worldbody>
    <body name="root"><freejoint/>
      <body name="child" pos="0 0 0" quat="0.7071067811865476 0 0 0.7071067811865476">
        <joint name="j" type="hinge" axis="0 0 1" range="-1 1"/>
        <inertial mass="1" diaginertia="1 1 1"/>
      </body>
    </body>
  </worldbody></mujoco>)");
  REQUIRE(result.ok());
  REQUIRE(result.model.joints.size() == 1u);
  CHECK(result.model.joints[0].origin_rpy[2] == Catch::Approx(1.5707963267948966).epsilon(1e-12));
}
