/**
 * @file test_rigid_body.cpp
 * @brief Unit tests for srsp::dynamics::RigidBodyDynamics.
 */
#include <srsp/dynamics/rigid_body.hpp>
#include <srsp/model/srm.hpp>

#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>

using Catch::Approx;
using srsp::dynamics::RigidBodyDynamics;

namespace {

constexpr double kGravity = 9.80665;

std::string models_dir() {
  const char* value = std::getenv("SRSP_TEST_MODELS");
  return value != nullptr ? std::string(value) : std::string();
}

srsp::model::Model load_model(const char* name) {
  const auto result = srsp::model::load_from_file(models_dir() + "/" + name);
  REQUIRE(result.ok());
  return result.model;
}

srsp::model::Model single_body(double mass, double ixx, double iyy, double izz) {
  const std::string document = R"({"schema":"srsp.model/1","name":"single","bodies":[{"id":"body",)"
                               R"("inertia":{"mass":)" +
                               std::to_string(mass) + R"(,"com":[0,0,0],"tensor":)" + "[" +
                               std::to_string(ixx) + ",0,0,0," + std::to_string(iyy) + ",0,0,0," +
                               std::to_string(izz) + R"(]}}]})";
  const auto result = srsp::model::load_from_json(document);
  REQUIRE(result.ok());
  return result.model;
}

}  // namespace

TEST_CASE("dynamics builds from an SRM model", "[dynamics]") {
  if (models_dir().empty()) {
    SKIP("SRSP_TEST_MODELS is not set");
  }
  const auto model = load_model("arm6.srm.json");
  const RigidBodyDynamics dynamics(model);
  CHECK(dynamics.joint_count() == 6);
  CHECK(dynamics.nq() == 13);
  CHECK(dynamics.nv() == 12);
  CHECK(dynamics.body_ids().size() == 7u);
}

TEST_CASE("neutral configuration keeps the base at the origin", "[dynamics]") {
  const RigidBodyDynamics dynamics(single_body(2.0, 0.1, 0.2, 0.3));
  const Eigen::VectorXd q = dynamics.neutral_configuration();
  const auto pose = dynamics.body_pose(q, "body");
  CHECK(pose.position.isZero());
  CHECK(pose.rotation.isApprox(Eigen::Matrix3d::Identity()));
}

TEST_CASE("floating body mass matrix is block diagonal", "[dynamics]") {
  const double mass = 3.0;
  const RigidBodyDynamics dynamics(single_body(mass, 0.5, 0.7, 0.9));
  const Eigen::VectorXd q = dynamics.neutral_configuration();
  const Eigen::MatrixXd matrix = dynamics.mass_matrix(q);
  REQUIRE(matrix.rows() == 6);
  REQUIRE(matrix.cols() == 6);

  CHECK(matrix.block<3, 3>(0, 0).isApprox(mass * Eigen::Matrix3d::Identity()));
  CHECK(matrix.block<3, 3>(3, 3).isApprox(
      (Eigen::Vector3d(0.5, 0.7, 0.9)).asDiagonal().toDenseMatrix()));
  CHECK(matrix.block<3, 3>(0, 3).norm() < 1e-12);
  CHECK(matrix.block<3, 3>(3, 0).norm() < 1e-12);
}

TEST_CASE("gravity produces weight and torque about the COM offset", "[dynamics]") {
  const RigidBodyDynamics dynamics(single_body(4.0, 1.0, 1.0, 1.0));
  const Eigen::VectorXd q = dynamics.neutral_configuration();
  const Eigen::VectorXd force = dynamics.gravity_force(q);
  CHECK(force[2] == Approx(-4.0 * kGravity).epsilon(1e-12));
  CHECK(force[0] == Approx(0.0).margin(1e-12));
  CHECK(force.tail<3>().norm() < 1e-12);
}

TEST_CASE("free fall accelerates at g", "[dynamics]") {
  const RigidBodyDynamics dynamics(single_body(1.0, 0.1, 0.1, 0.1));
  Eigen::VectorXd q = dynamics.neutral_configuration();
  Eigen::VectorXd v = Eigen::VectorXd::Zero(dynamics.nv());
  const Eigen::VectorXd tau = Eigen::VectorXd::Zero(dynamics.nv());

  const double dt = 1e-3;
  for (int i = 0; i < 100; ++i) {
    dynamics.step(&q, &v, tau, dt);
  }
  CHECK(v[2] == Approx(-kGravity * 0.1).epsilon(1e-9));

  // RK4 integrates constant acceleration exactly up to round-off.
  const double expected_drop = 0.5 * kGravity * 0.1 * 0.1;
  CHECK(q[2] == Approx(-expected_drop).epsilon(1e-9));
}

TEST_CASE("potential energy is m g h", "[dynamics]") {
  const RigidBodyDynamics dynamics(single_body(2.0, 0.1, 0.1, 0.1));
  Eigen::VectorXd q = dynamics.neutral_configuration();
  q[2] = 5.0;
  CHECK(dynamics.potential_energy(q) == Approx(2.0 * kGravity * 5.0).epsilon(1e-12));
}

TEST_CASE("jacobian matches finite differences of pose", "[dynamics]") {
  if (models_dir().empty()) {
    SKIP("SRSP_TEST_MODELS is not set");
  }
  const RigidBodyDynamics dynamics(load_model("arm6.srm.json"));
  Eigen::VectorXd q = dynamics.neutral_configuration();
  for (int i = 0; i < dynamics.joint_count(); ++i) {
    q[7 + i] = 0.1 * static_cast<double>(i + 1);
  }

  Eigen::VectorXd v = Eigen::VectorXd::Zero(dynamics.nv());
  for (int i = 0; i < dynamics.joint_count(); ++i) {
    v[6 + i] = 0.4 - 0.05 * static_cast<double>(i);
  }

  const Eigen::MatrixXd jac = dynamics.jacobian(q, "link6");
  const Eigen::Vector3d analytic = (jac * v).head<3>();

  // Central difference along the joint velocity direction; base velocity is
  // zero, so only the joint columns contribute.
  const double step = 1e-6;
  Eigen::VectorXd forward = q;
  Eigen::VectorXd backward = q;
  for (int i = 0; i < dynamics.joint_count(); ++i) {
    forward[7 + i] += v[6 + i] * step;
    backward[7 + i] -= v[6 + i] * step;
  }
  const Eigen::Vector3d finite_difference = (dynamics.body_pose(forward, "link6").position -
                                             dynamics.body_pose(backward, "link6").position) /
                                            (2.0 * step);

  for (int axis = 0; axis < 3; ++axis) {
    CHECK(analytic[axis] == Approx(finite_difference[axis]).margin(1e-6));
  }
}
TEST_CASE("kinetic energy uses the mass matrix", "[dynamics]") {
  const RigidBodyDynamics dynamics(single_body(2.0, 0.5, 0.5, 0.5));
  const Eigen::VectorXd q = dynamics.neutral_configuration();
  Eigen::VectorXd v = Eigen::VectorXd::Zero(dynamics.nv());
  v[0] = 1.5;
  v[3] = 2.0;
  const double expected = 0.5 * (2.0 * 1.5 * 1.5 + 0.5 * 4.0);
  CHECK(dynamics.kinetic_energy(q, v) == Approx(expected).epsilon(1e-12));
}

TEST_CASE("bias forces reduce to gravity at rest", "[dynamics]") {
  if (models_dir().empty()) {
    SKIP("SRSP_TEST_MODELS is not set");
  }
  const RigidBodyDynamics dynamics(load_model("arm6.srm.json"));
  Eigen::VectorXd q = dynamics.neutral_configuration();
  for (int i = 0; i < dynamics.joint_count(); ++i) {
    q[7 + i] = 0.2 * static_cast<double>(i + 1);
  }
  const Eigen::VectorXd v = Eigen::VectorXd::Zero(dynamics.nv());

  const Eigen::VectorXd at_rest = dynamics.bias_forces(q, v);
  const Eigen::VectorXd gravity = -dynamics.gravity_force(q);
  for (int i = 0; i < dynamics.nv(); ++i) {
    CHECK(at_rest[i] == Approx(gravity[i]).margin(1e-10));
  }
}

TEST_CASE("energy is conserved for a free arm", "[dynamics]") {
  if (models_dir().empty()) {
    SKIP("SRSP_TEST_MODELS is not set");
  }
  const RigidBodyDynamics dynamics(load_model("arm6.srm.json"));
  Eigen::VectorXd q = dynamics.neutral_configuration();
  for (int i = 0; i < dynamics.joint_count(); ++i) {
    q[7 + i] = 0.3 * static_cast<double>(i + 1);
  }
  Eigen::VectorXd v = Eigen::VectorXd::Zero(dynamics.nv());
  for (int i = 0; i < dynamics.joint_count(); ++i) {
    v[6 + i] = 0.2;
  }
  const Eigen::VectorXd tau = Eigen::VectorXd::Zero(dynamics.nv());

  const double initial = dynamics.kinetic_energy(q, v) + dynamics.potential_energy(q);
  const double dt = 1e-3;
  for (int step = 0; step < 2000; ++step) {  // 2 s
    dynamics.step(&q, &v, tau, dt);
  }
  const double final = dynamics.kinetic_energy(q, v) + dynamics.potential_energy(q);
  const double relative_drift = std::abs(final - initial) / std::abs(initial);
  INFO("initial=" << initial << " final=" << final);
  CHECK(relative_drift < 1e-3);
}

TEST_CASE("bias force includes the gyroscopic term", "[dynamics]") {
  // A single spinning body: h must contain w x (I w) in the angular block.
  const RigidBodyDynamics dynamics(single_body(2.0, 1.0, 2.0, 3.0));
  const Eigen::VectorXd q = dynamics.neutral_configuration();
  Eigen::VectorXd v = Eigen::VectorXd::Zero(dynamics.nv());
  v[3] = 1.0;  // angular velocity about world x
  v[4] = 2.0;

  const Eigen::VectorXd h = dynamics.bias_forces(q, v);
  const Eigen::Vector3d inertia(1.0, 2.0, 3.0);
  const Eigen::Vector3d omega(1.0, 2.0, 0.0);
  const Eigen::Vector3d expected = omega.cross(inertia.asDiagonal() * omega);
  CHECK(h[3] == Approx(expected[0]).margin(1e-9));
  CHECK(h[4] == Approx(expected[1]).margin(1e-9));
  CHECK(h[5] == Approx(expected[2]).margin(1e-9));
}

TEST_CASE("torque actuators saturate at max_effort", "[dynamics]") {
  if (models_dir().empty()) {
    SKIP("SRSP_TEST_MODELS is not set");
  }
  const RigidBodyDynamics dynamics(load_model("arm6.srm.json"));
  const Eigen::VectorXd q = dynamics.neutral_configuration();
  REQUIRE(dynamics.actuator_count() == 6);

  Eigen::VectorXd commands = Eigen::VectorXd::Zero(6);
  commands[0] = 100.0;  // arm6 a1 saturates at 30 N*m
  const Eigen::VectorXd tau = dynamics.actuator_force(q, commands);
  CHECK(tau[6] == Approx(30.0).epsilon(1e-12));

  commands[0] = -5.0;
  const Eigen::VectorXd small = dynamics.actuator_force(q, commands);
  CHECK(small[6] == Approx(-5.0).epsilon(1e-12));
}

TEST_CASE("wheel and thruster actuators act on the base body", "[dynamics]") {
  if (models_dir().empty()) {
    SKIP("SRSP_TEST_MODELS is not set");
  }
  const RigidBodyDynamics dynamics(load_model("sat_service.srm.json"));
  const Eigen::VectorXd q = dynamics.neutral_configuration();
  // Five actuators: three wheels on x/y/z, two thrusters along +-x.
  REQUIRE(dynamics.actuator_count() == 5);

  Eigen::VectorXd commands = Eigen::VectorXd::Zero(5);
  commands[0] = 0.1;  // wheel about body x
  const Eigen::VectorXd wheel = dynamics.actuator_force(q, commands);
  CHECK(wheel[3] == Approx(0.1).epsilon(1e-12));
  CHECK(wheel[0] == Approx(0.0).margin(1e-12));

  commands.setZero();
  commands[3] = 2.0;  // thruster along +x
  const Eigen::VectorXd thruster = dynamics.actuator_force(q, commands);
  CHECK(thruster[0] == Approx(2.0).epsilon(1e-12));
  CHECK(thruster[3] == Approx(0.0).margin(1e-12));
}

TEST_CASE("generalized force matches a Jacobian transpose", "[dynamics]") {
  if (models_dir().empty()) {
    SKIP("SRSP_TEST_MODELS is not set");
  }
  const RigidBodyDynamics dynamics(load_model("arm6.srm.json"));
  const Eigen::VectorXd q = dynamics.neutral_configuration();
  const Eigen::Vector3d force(0.0, 0.0, -4.0);
  const auto pose = dynamics.body_pose(q, "link6");
  const Eigen::VectorXd tau = dynamics.generalized_force(q, "link6", force, pose.position);
  const Eigen::VectorXd expected = dynamics.jacobian(q, "link6").topRows(3).transpose() * force;
  CHECK(tau.isApprox(expected, 1e-12));
}

TEST_CASE("two-body orbit stays on the circular solution", "[dynamics]") {
  // Point mass in a 7000 km circular orbit around a 3.986e14 m^3/s^2 body.
  const double mu = 3.986004418e14;
  const double radius = 7.0e6;
  const double speed = std::sqrt(mu / radius);

  RigidBodyDynamics dynamics(single_body(500.0, 100.0, 100.0, 100.0));
  dynamics.set_gravity(Eigen::Vector3d::Zero());

  Eigen::VectorXd q = dynamics.neutral_configuration();
  q[0] = radius;
  Eigen::VectorXd v = Eigen::VectorXd::Zero(dynamics.nv());
  v[1] = speed;

  const double dt = 1.0;
  constexpr double kPi = 3.14159265358979323846;
  const double period = 2.0 * kPi * std::sqrt(radius * radius * radius / mu);
  const int steps = static_cast<int>(period / dt);

  const auto two_body = [&dynamics, mu](const Eigen::VectorXd& q_state, const Eigen::VectorXd&) {
    const Eigen::Vector3d position(q_state[0], q_state[1], q_state[2]);
    const Eigen::Vector3d force = -mu * 500.0 * position / std::pow(position.norm(), 3.0);
    return dynamics.generalized_force(q_state, "body", force, position);
  };
  for (int step = 0; step < steps; ++step) {
    dynamics.step(&q, &v, two_body, dt);
  }

  const Eigen::Vector3d final_position(q[0], q[1], q[2]);
  INFO("radius error = " << (final_position.norm() - radius));
  CHECK(std::abs(final_position.norm() - radius) < 1.0);
  CHECK(final_position.x() == Approx(radius).margin(50.0));
}
TEST_CASE("angular velocity converts between frames", "[dynamics]") {
  // 90 deg about z: body x maps to world y.
  const RigidBodyDynamics dynamics(single_body(1.0, 1.0, 1.0, 1.0));
  Eigen::VectorXd q = dynamics.neutral_configuration();
  const double s = std::sqrt(0.5);
  q[3] = 0.0;
  q[4] = 0.0;
  q[5] = s;  // z
  q[6] = s;  // w

  const Eigen::Vector3d world_y(0.0, 1.0, 0.0);
  const Eigen::Vector3d body = dynamics.omega_world_to_body(q, world_y);
  CHECK(body.x() == Approx(1.0).margin(1e-12));
  CHECK(body.y() == Approx(0.0).margin(1e-12));

  const Eigen::Vector3d back = dynamics.omega_body_to_world(q, body);
  CHECK(back.isApprox(world_y, 1e-12));

  // The rotation itself: body x axis is world y under this yaw.
  const Eigen::Vector3d x_axis = dynamics.base_rotation(q) * Eigen::Vector3d(1.0, 0.0, 0.0);
  CHECK(x_axis.y() == Approx(1.0).margin(1e-12));
}
