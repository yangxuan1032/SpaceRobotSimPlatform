/**
 * @file test_aba_isolation.cpp
 * @brief Isolation harness for the experimental ABA (no assertions on tolerance).
 *
 * Prints the residual of the ABA acceleration against the reference solution for
 * two regimes so the failing term can be identified: zero velocity (all
 * velocity-product terms vanish) and zero torque (velocity terms dominate).
 */
#include <srsp/dynamics/rigid_body.hpp>
#include <srsp/model/srm.hpp>

#include <Eigen/Cholesky>
#include <Eigen/Core>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <random>

using srsp::dynamics::RigidBodyDynamics;

namespace {

const char* kOneJoint = R"({
  "schema": "srsp.model/1",
  "name": "one_joint",
  "bodies": [
    {"id": "base", "inertia": {"mass": 10.0, "com": [0,0,0],
      "tensor": [1.0,0,0,0,1.0,0,0,0,1.0]}},
    {"id": "link", "parent_joint": "j1", "inertia": {"mass": 2.0, "com": [0,0,0.3],
      "tensor": [0.2,0,0,0,0.2,0,0,0,0.05]}}
  ],
  "joints": [
    {"id": "j1", "parent": "base", "child": "link", "type": "revolute", "axis": [0,0,1],
     "lower": -3.14, "upper": 3.14}
  ]
})";

RigidBodyDynamics make_model() {
  const auto result = srsp::model::load_from_json(kOneJoint);
  REQUIRE(result.ok());
  return RigidBodyDynamics(result.model);
}

double residual(const RigidBodyDynamics& dynamics, const Eigen::VectorXd& q,
                const Eigen::VectorXd& v, const Eigen::VectorXd& tau) {
  const Eigen::MatrixXd mass = dynamics.mass_matrix(q);
  const Eigen::VectorXd bias = dynamics.bias_forces(q, v);
  Eigen::LDLT<Eigen::MatrixXd> ldlt(mass);
  const Eigen::VectorXd reference = ldlt.solve(tau - bias);
  const Eigen::VectorXd aba = dynamics.aba_accelerations(q, v, tau);
  return (aba - reference).norm();
}

}  // namespace

TEST_CASE("ABA isolation probe", "[dynamics][aba]") {
  const RigidBodyDynamics dynamics = make_model();
  std::mt19937 rng(11);
  std::uniform_real_distribution<double> angle(-0.5, 0.5);
  std::uniform_real_distribution<double> rate(-0.6, 0.6);
  std::uniform_real_distribution<double> torque(-2.0, 2.0);

  Eigen::VectorXd q = dynamics.neutral_configuration();
  for (int i = 0; i < q.size(); ++i) {
    q[i] += angle(rng);
  }
  Eigen::VectorXd v = Eigen::VectorXd::Zero(dynamics.nv());
  for (int i = 0; i < v.size(); ++i) {
    v[i] = rate(rng);
  }
  Eigen::VectorXd tau = Eigen::VectorXd::Zero(dynamics.nv());
  for (int i = 0; i < tau.size(); ++i) {
    tau[i] = torque(rng);
  }

  const Eigen::VectorXd zero = Eigen::VectorXd::Zero(dynamics.nv());
  std::printf("RESIDUAL v=0 tau!=0 : %.6g\n", residual(dynamics, q, zero, tau));
  std::printf("RESIDUAL v!=0 tau=0 : %.6g\n", residual(dynamics, q, v, zero));
  std::printf("RESIDUAL v!=0 tau!=0: %.6g\n", residual(dynamics, q, v, tau));
  CHECK(dynamics.aba_accelerations(q, v, tau).size() == dynamics.nv());
}