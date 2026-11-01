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
