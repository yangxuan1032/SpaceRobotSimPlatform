/**
 * @file test_force_models.cpp
 * @brief Unit tests for the environmental force models.
 */
#include <srsp/dynamics/force_models.hpp>

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace srsp::dynamics;

TEST_CASE("two-body gravity follows the inverse square law", "[force]") {
  const double mu = ForceConstants::kEarthMu;
  const double mass = 1200.0;
  const double radius = 7.0e6;
  const Eigen::Vector3d position(radius, 0.0, 0.0);

  const Eigen::Vector3d force = two_body_gravity(mu, mass, position);
  const double expected = mu * mass / (radius * radius);
  CHECK(force.norm() == Approx(expected).epsilon(1e-12));
  CHECK(force.x() == Approx(-expected).epsilon(1e-12));
  CHECK(force.y() == Approx(0.0).margin(1e-12));
}

TEST_CASE("third body points towards the perturbing body", "[force]") {
  const Eigen::Vector3d body(7.0e6, 0.0, 0.0);
  const Eigen::Vector3d sun(ForceConstants::kAstronomicalUnit, 0.0, 0.0);
  const Eigen::Vector3d force = third_body(ForceConstants::kSunMu, 500.0, body, sun);
  CHECK(force.x() > 0.0);
  CHECK(force.y() == Approx(0.0).margin(1e-12));

  const double distance = sun.x() - body.x();
  const double expected = ForceConstants::kSunMu * 500.0 / (distance * distance);
  CHECK(force.x() == Approx(expected).epsilon(1e-9));
}

TEST_CASE("solar radiation pressure scales with area and reflectivity", "[force]") {
  SolarPressure plate;
  plate.area = 2.0;
  plate.reflectivity = 1.0;
  const Eigen::Vector3d direction(1.0, 0.0, 0.0);
  const Eigen::Vector3d force = solar_radiation_pressure(plate, direction);

  const double pressure = ForceConstants::kSolarFlux / ForceConstants::kSpeedOfLight;
  const double expected = pressure * 2.0 * 2.0;
  CHECK(force.norm() == Approx(expected).epsilon(1e-12));
  CHECK(force.x() == Approx(expected).epsilon(1e-12));

  plate.in_eclipse = true;
  CHECK(solar_radiation_pressure(plate, direction).isZero());
}

TEST_CASE("atmosphere density decays exponentially", "[force]") {
  Atmosphere atmosphere;
  atmosphere.density_at_reference = 1.0e-12;
  atmosphere.reference_altitude = 400.0e3;
  atmosphere.scale_height = 8.5e3;

  CHECK(atmosphere.density(400.0e3) == Approx(1.0e-12).epsilon(1e-12));
  CHECK(atmosphere.density(408.5e3) == Approx(1.0e-12 * std::exp(-1.0)).epsilon(1e-12));
}

TEST_CASE("drag opposes the velocity direction", "[force]") {
  Atmosphere atmosphere;
  atmosphere.density_at_reference = 1.0e-11;
  atmosphere.reference_altitude = 400.0e3;
  atmosphere.drag_coefficient = 2.2;
  atmosphere.area = 3.0;

  const Eigen::Vector3d position(ForceConstants::kEarthRadius + 400.0e3, 0.0, 0.0);
  const Eigen::Vector3d velocity(0.0, 7660.0, 0.0);
  const Eigen::Vector3d force = atmospheric_drag(atmosphere, 900.0, position, velocity);

  CHECK(force.x() == Approx(0.0).margin(1e-15));
  CHECK(force.y() < 0.0);

  const double speed = velocity.norm();
  const double expected = 0.5 * atmosphere.density(400.0e3) * speed * speed * 2.2 * 3.0;
  CHECK(force.norm() == Approx(expected).epsilon(1e-9));
}

TEST_CASE("drag vanishes in vacuum", "[force]") {
  Atmosphere atmosphere;  // density_at_reference defaults to zero
  const Eigen::Vector3d force = atmospheric_drag(
      atmosphere, 900.0, Eigen::Vector3d(7.0e6, 0.0, 0.0), Eigen::Vector3d(0.0, 7000.0, 0.0));
  CHECK(force.isZero());
}
