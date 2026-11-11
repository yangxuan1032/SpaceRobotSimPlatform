/**
 * @file test_environment.cpp
 * @brief W4: celestial sampling, shadow geometry and RK4 two-body propagation.
 */
#include <srsp/core/environment.hpp>

#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <limits>

using srsp::core::CelestialBody;
using srsp::core::circular_speed;
using srsp::core::EnvironmentSpec;
using srsp::core::evaluate_environment;
using srsp::core::norm;
using srsp::core::orbital_period;
using srsp::core::OrbitState;
using srsp::core::propagate_orbit;

namespace {

constexpr double kEarthMu = 3.986004418e14;
constexpr double kEarthRadius = 6.371e6;
constexpr double kAu = 1.495978707e11;

EnvironmentSpec make_spec() {
  EnvironmentSpec spec;
  spec.earth.id = "earth";
  spec.earth.mu = kEarthMu;
  spec.earth.radius_m = kEarthRadius;
  spec.earth.reference_radius_m = 6.378e6;
  spec.sun.id = "sun";
  spec.sun.mu = 1.32712440018e20;
  spec.sun.radius_m = 6.957e8;
  spec.sun.position_m = {kAu, 0.0, 0.0};
  spec.moon.id = "moon";
  spec.moon.mu = 4.9048695e12;
  spec.moon.radius_m = 1.737e6;
  spec.moon.position_m = {3.844e8, 0.0, 0.0};
  return spec;
}

}  // namespace

TEST_CASE("circular speed and period match the analytic values", "[environment]") {
  const double radius = 7.0e6;
  const double speed = circular_speed(kEarthMu, radius);
  CHECK(std::abs(speed - std::sqrt(kEarthMu / radius)) < 1e-9);
  const double period = orbital_period(kEarthMu, radius);
  CHECK(std::abs(period - 2.0 * 3.14159265358979323846 *
                              std::sqrt(radius * radius * radius / kEarthMu)) < 1e-9);
}

TEST_CASE("a circular orbit closes after one period", "[environment]") {
  const EnvironmentSpec spec = make_spec();
  const double radius = 7.0e6;
  OrbitState state;
  state.position_m = {radius, 0.0, 0.0};
  state.velocity_m_s = {0.0, circular_speed(kEarthMu, radius), 0.0};

  const double period = orbital_period(kEarthMu, radius);
  const int steps = 2000;
  const double dt = period / static_cast<double>(steps);
  for (int i = 0; i < steps; ++i) {
    REQUIRE(propagate_orbit(spec, &state, dt));
  }

  const srsp::core::Vec3 start{radius, 0.0, 0.0};
  const double drift = norm({state.position_m[0] - start[0], state.position_m[1] - start[1],
                             state.position_m[2] - start[2]});
  std::printf("orbit closure drift=%.3e m over %.1f s\n", drift, period);
  CHECK(drift / radius < 1e-6);
  CHECK(std::abs(norm(state.position_m) - radius) / radius < 1e-6);
}

TEST_CASE("the sun direction and flux follow the geometry", "[environment]") {
  const EnvironmentSpec spec = make_spec();
  const auto sample = evaluate_environment(spec, {0.0, 0.0, 0.0});
  CHECK(std::abs(sample.sun_direction[0] - 1.0) < 1e-12);
  CHECK(std::abs(sample.solar_flux_w_m2 - 1361.0) < 1.0);
  CHECK_FALSE(sample.eclipse);
}

TEST_CASE("an entity behind the earth is in umbra", "[environment]") {
  const EnvironmentSpec spec = make_spec();
  // Sun at +x, entity at -x: the earth sits between them.
  const auto dark = evaluate_environment(spec, {-7.0e6, 0.0, 0.0});
  CHECK(dark.eclipse);
  CHECK(dark.visibility == 0.0);

  // Sun and entity on the same side: fully lit.
  const auto lit = evaluate_environment(spec, {7.0e6, 0.0, 0.0});
  CHECK_FALSE(lit.eclipse);
  CHECK(lit.visibility == 1.0);
}

TEST_CASE("third bodies and J2 change the acceleration", "[environment]") {
  EnvironmentSpec two_body = make_spec();
  const auto base = evaluate_environment(two_body, {7.0e6, 0.0, 0.0});

  EnvironmentSpec with_third = make_spec();
  with_third.use_third_body = true;
  const auto third = evaluate_environment(with_third, {7.0e6, 0.0, 0.0});
  CHECK(norm({third.gravity_m_s2[0] - base.gravity_m_s2[0],
              third.gravity_m_s2[1] - base.gravity_m_s2[1],
              third.gravity_m_s2[2] - base.gravity_m_s2[2]}) > 0.0);

  EnvironmentSpec with_j2 = make_spec();
  with_j2.use_j2 = true;
  with_j2.earth.j2 = 1.08263e-3;
  const auto j2 = evaluate_environment(with_j2, {0.0, 0.0, 7.0e6});
  const auto none = evaluate_environment(two_body, {0.0, 0.0, 7.0e6});
  CHECK(std::abs(j2.gravity_m_s2[2] - none.gravity_m_s2[2]) > 1e-6);
}

TEST_CASE("propagation rejects invalid steps", "[environment]") {
  const EnvironmentSpec spec = make_spec();
  OrbitState state;
  state.position_m = {7.0e6, 0.0, 0.0};
  state.velocity_m_s = {0.0, circular_speed(kEarthMu, 7.0e6), 0.0};
  CHECK_FALSE(propagate_orbit(spec, &state, 0.0));
  CHECK_FALSE(propagate_orbit(spec, &state, -1.0));
  CHECK_FALSE(propagate_orbit(spec, &state, std::numeric_limits<double>::quiet_NaN()));
  CHECK_FALSE(propagate_orbit(spec, nullptr, 1.0));
}
TEST_CASE("a celestial manifest yields a body with physical parameters", "[environment]") {
  const std::string manifest = R"({
    "schema": "srsp.component/1",
    "kind": "env.celestial",
    "id": "org.srsp.env.earth",
    "params": { "type": "object", "properties": {
      "mu": {"type": "number", "default": 3.986004418e14},
      "radius_m": {"type": "number", "default": 6371000.0},
      "j2": {"type": "number", "default": 1.08263e-3},
      "reference_radius_m": {"type": "number", "default": 6378000.0},
      "ephemeris": {"type": "string", "default": "fixed:0,0,0"}
    } }
  })";
  srsp::core::CelestialBody body;
  std::string error;
  REQUIRE(srsp::core::celestial_from_manifest(manifest, &body, &error));
  CHECK(body.mu == 3.986004418e14);
  CHECK(body.radius_m == 6371000.0);
  CHECK(body.j2 == 1.08263e-3);
  CHECK(body.reference_radius_m == 6378000.0);
}

TEST_CASE("a celestial manifest without mu is rejected", "[environment]") {
  const std::string manifest = R"({
    "schema": "srsp.component/1",
    "kind": "env.celestial",
    "id": "org.srsp.env.bad",
    "params": { "type": "object", "properties": {
      "radius_m": {"type": "number", "default": 6371000.0}
    } }
  })";
  srsp::core::CelestialBody body;
  std::string error;
  CHECK_FALSE(srsp::core::celestial_from_manifest(manifest, &body, &error));
  CHECK(error.find("mu") != std::string::npos);

  const std::string wrong_kind = R"({"kind": "dynamics.body", "id": "x", "params": {}})";
  CHECK_FALSE(srsp::core::celestial_from_manifest(wrong_kind, &body, &error));
  CHECK(error.find("env.celestial") != std::string::npos);
}

TEST_CASE("solar radiation pressure follows flux, area and shadow", "[environment]") {
  EnvironmentSpec spec = make_spec();
  const auto lit = evaluate_environment(spec, {0.0, 0.0, 0.0});
  const auto acceleration = srsp::core::solar_radiation_pressure_acceleration(lit, 2.0, 100.0, 1.5);
  const double expected = 1.5 * 2.0 / 100.0 * (lit.solar_flux_w_m2 / 2.99792458e8);
  CHECK(std::abs(srsp::core::norm(acceleration) - expected) < expected * 1e-9 + 1e-15);
  // Pushed away from the sun: the sun is at +x, so the acceleration is -x.
  CHECK(acceleration[0] < 0.0);

  const auto dark = evaluate_environment(spec, {-7.0e6, 0.0, 0.0});
  const auto shadowed = srsp::core::solar_radiation_pressure_acceleration(dark, 2.0, 100.0, 1.5);
  CHECK(srsp::core::norm(shadowed) == 0.0);
}

TEST_CASE("gravity-gradient torque vanishes on a principal axis", "[environment]") {
  const std::array<double, 9> inertia{100.0, 0.0, 0.0, 0.0, 120.0, 0.0, 0.0, 0.0, 140.0};
  const auto aligned = srsp::core::gravity_gradient_torque(kEarthMu, {0.0, 0.0, 7.0e6}, inertia);
  CHECK(srsp::core::norm(aligned) < 1e-12);

  const auto tilted =
      srsp::core::gravity_gradient_torque(kEarthMu, {4.949747468e6, 0.0, 4.949747468e6}, inertia);
  CHECK(srsp::core::norm(tilted) > 1e-9);
}