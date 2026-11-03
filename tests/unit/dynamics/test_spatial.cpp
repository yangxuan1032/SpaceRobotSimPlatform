/**
 * @file test_spatial.cpp
 * @brief Unit tests for the spatial algebra primitives used by ABA.
 */
#include <cmath>

#include <Eigen/Core>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <random>

#include "spatial.hpp"

using srsp::dynamics::bias_acceleration;
using srsp::dynamics::bias_force;
using srsp::dynamics::kinetic_energy;
using srsp::dynamics::make_spatial_inertia;
using srsp::dynamics::SpatialVector;

namespace {
constexpr double kTolerance = 1e-12;
}  // namespace

TEST_CASE("spatial inertia reproduces the kinetic energy", "[dynamics][spatial]") {
  const double mass = 7.5;
  const Eigen::Vector3d com(0.2, -0.1, 0.35);
  Eigen::Matrix3d rotational = Eigen::Matrix3d::Zero();
  rotational.diagonal() << 0.4, 0.5, 0.6;

  std::mt19937 rng(7);
  std::uniform_real_distribution<double> value(-1.0, 1.0);
  for (int trial = 0; trial < 10; ++trial) {
    SpatialVector velocity;
    for (int i = 0; i < 6; ++i) {
      velocity[i] = value(rng);
    }
    const Eigen::Vector3d v = velocity.head<3>();
    const Eigen::Vector3d w = velocity.tail<3>();
    const double expected =
        0.5 * mass * (v + w.cross(com)).squaredNorm() + 0.5 * w.dot(rotational * w);
    const double actual = kinetic_energy(make_spatial_inertia(mass, com, rotational), velocity);
    CHECK(std::abs(actual - expected) <= kTolerance * (1.0 + std::abs(expected)));
  }
}

TEST_CASE("spatial inertia is symmetric and positive definite", "[dynamics][spatial]") {
  const auto inertia = make_spatial_inertia(3.0, Eigen::Vector3d(0.1, 0.2, 0.0),
                                            Eigen::Vector3d(1.0, 2.0, 3.0).asDiagonal());
  CHECK(inertia.isApprox(inertia.transpose(), 1e-12));
  Eigen::LDLT<Eigen::Matrix<double, 6, 6>> ldlt(inertia);
  CHECK(ldlt.info() == Eigen::Success);
  CHECK(ldlt.vectorD().minCoeff() > 0.0);
}

TEST_CASE("bias terms vanish for pure rotation about the origin", "[dynamics][spatial]") {
  SpatialVector velocity = SpatialVector::Zero();
  velocity.tail<3>() << 0.3, -0.7, 1.1;
  CHECK(bias_acceleration(velocity).norm() <= 1e-15);

  // v dot (v x* f) == 0 for any force, so the bias power is zero.
  SpatialVector momentum;
  momentum << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;
  const SpatialVector force = bias_force(velocity, momentum);
  CHECK(std::abs(velocity.dot(force)) <= 1e-12);
}