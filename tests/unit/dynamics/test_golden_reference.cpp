/**
 * @file test_golden_reference.cpp
 * @brief Golden reference for the dynamics: M(q) a + h(q, v) = tau.
 *
 * This test pins the equation of motion against the mass matrix and bias terms
 * before any O(n) algorithm (CRBA/ABA) replaces the generalised inverse, so a
 * faster solver can be validated against the same reference later.
 */
#include <srsp/dynamics/rigid_body.hpp>
#include <srsp/model/srm.hpp>

#include <string>

#include <Eigen/Cholesky>
#include <Eigen/Core>
#include <catch2/catch_test_macros.hpp>
#include <random>

#include "chain6_oracle_reference.hpp"

using srsp::dynamics::RigidBodyDynamics;

namespace {

const char* kChain = R"({
  "schema": "srsp.model/1",
  "name": "chain6",
  "bodies": [
    {"id": "base", "inertia": {"mass": 120.0, "com": [0,0,0],
      "tensor": [8,0,0,0,8,0,0,0,6]}},
    {"id": "l1", "parent_joint": "j1", "inertia": {"mass": 12.0, "com": [0,0,0.25],
      "tensor": [0.35,0,0,0,0.35,0,0,0,0.05]}},
    {"id": "l2", "parent_joint": "j2", "inertia": {"mass": 10.0, "com": [0,0,0.3],
      "tensor": [0.42,0,0,0,0.42,0,0,0,0.04]}},
    {"id": "l3", "parent_joint": "j3", "inertia": {"mass": 8.0, "com": [0,0,0.25],
      "tensor": [0.25,0,0,0,0.25,0,0,0,0.03]}},
    {"id": "l4", "parent_joint": "j4", "inertia": {"mass": 6.0, "com": [0,0,0.2],
      "tensor": [0.12,0,0,0,0.12,0,0,0,0.02]}},
    {"id": "l5", "parent_joint": "j5", "inertia": {"mass": 4.0, "com": [0,0,0.15],
      "tensor": [0.06,0,0,0,0.06,0,0,0,0.01]}},
    {"id": "l6", "parent_joint": "j6", "inertia": {"mass": 2.0, "com": [0,0,0.1],
      "tensor": [0.02,0,0,0,0.02,0,0,0,0.005]}}
  ],
  "joints": [
    {"id": "j1", "parent": "base", "child": "l1", "type": "revolute", "axis": [0,0,1],
     "lower": -3.14, "upper": 3.14},
    {"id": "j2", "parent": "l1", "child": "l2", "type": "revolute", "axis": [0,1,0],
     "lower": -2.0, "upper": 2.0, "origin": [0,0,0.35]},
    {"id": "j3", "parent": "l2", "child": "l3", "type": "revolute", "axis": [0,1,0],
     "lower": -2.5, "upper": 2.5, "origin": [0,0,0.4]},
    {"id": "j4", "parent": "l3", "child": "l4", "type": "revolute", "axis": [0,0,1],
     "lower": -3.14, "upper": 3.14, "origin": [0,0,0.35]},
    {"id": "j5", "parent": "l4", "child": "l5", "type": "revolute", "axis": [0,1,0],
     "lower": -2.0, "upper": 2.0, "origin": [0,0,0.3]},
    {"id": "j6", "parent": "l5", "child": "l6", "type": "revolute", "axis": [0,0,1],
     "lower": -3.14, "upper": 3.14, "origin": [0,0,0.2]}
  ]
})";

RigidBodyDynamics make_chain() {
  const auto result = srsp::model::load_from_json(kChain);
  REQUIRE(result.ok());
  return RigidBodyDynamics(result.model);
}

}  // namespace

TEST_CASE("mass matrix is symmetric positive definite", "[dynamics][golden]") {
  const RigidBodyDynamics dynamics = make_chain();
  Eigen::VectorXd q = dynamics.neutral_configuration();
  q.tail(q.size() - 1).setConstant(0.3);
  const Eigen::MatrixXd mass = dynamics.mass_matrix(q);
  CHECK(mass.isApprox(mass.transpose(), 1e-10));
  Eigen::LDLT<Eigen::MatrixXd> ldlt(mass);
  CHECK(ldlt.info() == Eigen::Success);
}

TEST_CASE("golden reference satisfies M a + h = tau", "[dynamics][golden]") {
  const RigidBodyDynamics dynamics = make_chain();
  std::mt19937 rng(20261101);
  std::uniform_real_distribution<double> angle(-0.6, 0.6);
  std::uniform_real_distribution<double> rate(-0.4, 0.4);
  std::uniform_real_distribution<double> torque(-2.0, 2.0);

  const int nv = dynamics.nv();
  for (int trial = 0; trial < 5; ++trial) {
    Eigen::VectorXd q = dynamics.neutral_configuration();
    Eigen::VectorXd v(nv);
    Eigen::VectorXd tau(nv);
    for (int i = 0; i < q.size(); ++i) {
      q[i] += angle(rng);
    }
    for (int i = 0; i < nv; ++i) {
      v[i] = rate(rng);
      tau[i] = torque(rng);
    }

    const Eigen::MatrixXd mass = dynamics.mass_matrix(q);
    const Eigen::VectorXd bias = dynamics.bias_forces(q, v);
    Eigen::LDLT<Eigen::MatrixXd> ldlt(mass);
    REQUIRE(ldlt.info() == Eigen::Success);
    const Eigen::VectorXd accel = ldlt.solve(tau - bias);
    CHECK((mass * accel + bias - tau).norm() <= 1e-9 * (1.0 + tau.norm()));

    // The RK4 integrator must agree with that acceleration for a tiny step.
    const double dt = 1e-6;
    Eigen::VectorXd q_next = q;
    Eigen::VectorXd v_next = v;
    dynamics.step(&q_next, &v_next, tau, dt);
    const Eigen::VectorXd measured = (v_next - v) / dt;
    const double tolerance = 1e-4 * (1.0 + accel.norm());
    INFO("trial=" << trial << " residual=" << (measured - accel).norm());
    CHECK((measured - accel).norm() <= tolerance);
  }
}
TEST_CASE("optimised assembly matches the reference mass matrix", "[dynamics][golden]") {
  const RigidBodyDynamics dynamics = make_chain();
  std::mt19937 rng(20261101);
  std::uniform_real_distribution<double> angle(-0.6, 0.6);

  for (int trial = 0; trial < 5; ++trial) {
    Eigen::VectorXd q = dynamics.neutral_configuration();
    for (int i = 0; i < q.size(); ++i) {
      q[i] += angle(rng);
    }
    const Eigen::MatrixXd fast = dynamics.mass_matrix(q);
    const Eigen::MatrixXd reference = dynamics.mass_matrix_reference(q);
    INFO("trial=" << trial << " max_diff=" << (fast - reference).cwiseAbs().maxCoeff());
    CHECK(fast.isApprox(reference, 1e-10));
  }
}
TEST_CASE("CRBA matches the reference mass matrix", "[dynamics][golden]") {
  const RigidBodyDynamics dynamics = make_chain();
  std::mt19937 rng(20261102);
  std::uniform_real_distribution<double> angle(-0.6, 0.6);
  for (int trial = 0; trial < 5; ++trial) {
    Eigen::VectorXd q = dynamics.neutral_configuration();
    for (int i = 0; i < q.size(); ++i) {
      q[i] += angle(rng);
    }
    const Eigen::MatrixXd crba = dynamics.mass_matrix_crba(q);
    const Eigen::MatrixXd reference = dynamics.mass_matrix_reference(q);
    const double diff = (crba - reference).cwiseAbs().maxCoeff();
    std::printf("CRBA trial=%d max_diff=%.6g\n", trial, diff);
    CHECK(crba.isApprox(reference, 1e-10));
  }
}
TEST_CASE("semi-implicit integrator is selectable and deterministic", "[dynamics]") {
  RigidBodyDynamics dynamics = make_chain();
  CHECK(dynamics.integrator() == srsp::dynamics::IntegratorKind::kRk4);

  auto run = [&](srsp::dynamics::IntegratorKind kind) {
    dynamics.set_integrator(kind);
    Eigen::VectorXd q = dynamics.neutral_configuration();
    Eigen::VectorXd v = Eigen::VectorXd::Zero(dynamics.nv());
    const Eigen::VectorXd tau = Eigen::VectorXd::Zero(dynamics.nv());
    q[7] = 0.2;
    for (int i = 0; i < 10; ++i) {
      dynamics.step(&q, &v, tau, 1e-4);
    }
    return std::make_pair(q, v);
  };

  const auto first = run(srsp::dynamics::IntegratorKind::kSemiImplicit);
  const auto second = run(srsp::dynamics::IntegratorKind::kSemiImplicit);
  CHECK(first.first.isApprox(second.first, 0.0));  // bit-identical
  CHECK(first.second.isApprox(second.second, 0.0));

  const auto rk4 = run(srsp::dynamics::IntegratorKind::kRk4);
  // Both integrate the same dynamics; at this step size they stay close.
  CHECK((first.first - rk4.first).norm() < 1e-2);
  dynamics.set_integrator(srsp::dynamics::IntegratorKind::kRk4);
}
TEST_CASE("integrator divergence stays bounded", "[dynamics][integrator]") {
  // The semi-implicit mode is documented as a first-order performance profile;
  // this test pins how far it may drift from RK4 on the benchmark chain so a
  // future change cannot silently make the fast profile inaccurate.
  RigidBodyDynamics dynamics = make_chain();
  Eigen::VectorXd q0 = dynamics.neutral_configuration();
  Eigen::VectorXd v0 = Eigen::VectorXd::Zero(dynamics.nv());
  const Eigen::VectorXd tau = Eigen::VectorXd::Zero(dynamics.nv());
  for (int i = 0; i < dynamics.joint_count(); ++i) {
    q0[7 + i] = 0.05;
  }

  auto integrate = [&](srsp::dynamics::IntegratorKind kind) {
    dynamics.set_integrator(kind);
    Eigen::VectorXd q = q0;
    Eigen::VectorXd v = v0;
    for (int step = 0; step < 200; ++step) {
      dynamics.step(&q, &v, tau, 1e-3);
    }
    return std::make_pair(q, v);
  };

  const auto rk4 = integrate(srsp::dynamics::IntegratorKind::kRk4);
  const auto semi = integrate(srsp::dynamics::IntegratorKind::kSemiImplicit);
  const double position_drift = (semi.first.head(3) - rk4.first.head(3)).norm();
  const double joint_drift = (semi.first.tail(7) - rk4.first.tail(7)).norm();
  std::printf("integrator divergence position=%.6g joints=%.6g\n", position_drift, joint_drift);
  CHECK(position_drift < 5e-3);
  CHECK(joint_drift < 5e-3);
  dynamics.set_integrator(srsp::dynamics::IntegratorKind::kRk4);
}
TEST_CASE("mass matrix and bias match the MuJoCo oracle (six joints)", "[dynamics][oracle]") {
  RigidBodyDynamics dynamics = make_chain();
  dynamics.set_gravity(Eigen::Vector3d(0.0, 0.0, -9.81));

  Eigen::VectorXd q(dynamics.nq());
  for (int i = 0; i < q.size(); ++i) {
    q[i] = srsp::test::chain6::kQ[i];
  }
  Eigen::VectorXd v(dynamics.nv());
  for (int i = 0; i < v.size(); ++i) {
    v[i] = srsp::test::chain6::kV[i];
  }

  const Eigen::MatrixXd mass = dynamics.mass_matrix(q);
  const Eigen::VectorXd bias = dynamics.bias_forces(q, v);

  REQUIRE(mass.rows() == srsp::test::chain6::kNv);
  REQUIRE(bias.size() == srsp::test::chain6::kNv);

  auto close = [](double actual, double expected) {
    const double scale = std::max(1.0, std::abs(expected));
    return std::abs(actual - expected) <= 1e-9 * scale;
  };

  int mass_mismatches = 0;
  double worst_mass = 0.0;
  for (int row = 0; row < mass.rows(); ++row) {
    for (int col = 0; col < mass.cols(); ++col) {
      const double expected = srsp::test::chain6::kMass[row][col];
      if (!close(mass(row, col), expected)) {
        ++mass_mismatches;
        worst_mass = std::max(worst_mass, std::abs(mass(row, col) - expected));
      }
    }
  }

  int bias_mismatches = 0;
  double worst_bias = 0.0;
  for (int i = 0; i < bias.size(); ++i) {
    const double expected = srsp::test::chain6::kBias[i];
    if (!close(bias[i], expected)) {
      ++bias_mismatches;
      worst_bias = std::max(worst_bias, std::abs(bias[i] - expected));
    }
  }

  std::printf("oracle chain6 mass_mismatches=%d worst=%.6g bias_mismatches=%d worst=%.6g\n",
              mass_mismatches, worst_mass, bias_mismatches, worst_bias);
  CHECK(mass_mismatches == 0);
  CHECK(bias_mismatches == 0);
}
