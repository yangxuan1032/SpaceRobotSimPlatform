/**
 * @file test_world.cpp
 * @brief W3: multi-entity world, atomic commit and snapshot publication.
 */
#include <srsp/core/environment.hpp>
#include <srsp/core/world.hpp>

#include <cmath>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <limits>

using srsp::core::SnapshotStore;
using srsp::core::World;
using srsp::core::WorldCommand;
using srsp::core::WorldEntity;

namespace {

WorldEntity make_entity(const char* id, double x, double vx) {
  WorldEntity entity;
  entity.id = id;
  entity.position_m = {x, 0.0, 0.0};
  entity.velocity_m_s = {vx, 0.0, 0.0};
  return entity;
}

double decode_position(const SnapshotStore::Lease& lease, std::size_t index) {
  double value = 0.0;
  const std::size_t offset = World::kSnapshotHeaderBytes + index * World::kBytesPerEntity + 64;
  std::memcpy(&value, lease.bytes().data() + offset, sizeof(double));
  return value;
}

}  // namespace

TEST_CASE("two entities share one snapshot per step", "[world]") {
  World world;
  std::string error;
  REQUIRE(world.add_entity(make_entity("entity/satellite", 0.0, 1.0), &error));
  REQUIRE(world.add_entity(make_entity("entity/probe", 10.0, -2.0), &error));
  CHECK(world.entities().size() == 2u);  // sorted: probe, satellite

  REQUIRE(world.step(0.5, &error));
  CHECK(world.step_index() == 1u);
  CHECK(world.latest_id().generation == 1u);

  const auto lease = world.acquire_latest();
  REQUIRE(lease.has_value());
  CHECK(lease->bytes().size() == World::kSnapshotHeaderBytes + 2u * World::kBytesPerEntity);
  // Sorted order: "entity/probe" first.
  CHECK(decode_position(*lease, 0u) == 9.0);
  CHECK(decode_position(*lease, 1u) == 0.5);
}

TEST_CASE("duplicate entity ids are rejected", "[world]") {
  World world;
  std::string error;
  REQUIRE(world.add_entity(make_entity("entity/a", 0.0, 0.0), &error));
  CHECK_FALSE(world.add_entity(make_entity("entity/a", 1.0, 0.0), &error));
  CHECK(error.find("duplicate") != std::string::npos);
  CHECK(world.entities().size() == 1u);
}

TEST_CASE("commands only move their target", "[world]") {
  World world;
  std::string error;
  REQUIRE(world.add_entity(make_entity("entity/a", 0.0, 0.0), &error));
  REQUIRE(world.add_entity(make_entity("entity/b", 0.0, 0.0), &error));

  WorldCommand command;
  command.entity = "entity/b";
  command.delta_velocity_m_s = {3.0, 0.0, 0.0};
  world.push_command(command);

  REQUIRE(world.step(1.0, &error));
  const auto* a = world.find("entity/a");
  const auto* b = world.find("entity/b");
  REQUIRE(a != nullptr);
  REQUIRE(b != nullptr);
  CHECK(a->velocity_m_s[0] == 0.0);
  CHECK(a->position_m[0] == 0.0);
  CHECK(b->velocity_m_s[0] == 3.0);
  CHECK(b->position_m[0] == 3.0);
}

TEST_CASE("a bad command leaves state and generation untouched", "[world]") {
  World world;
  std::string error;
  REQUIRE(world.add_entity(make_entity("entity/a", 1.0, 0.0), &error));
  REQUIRE(world.step(1.0, &error));
  const auto generation = world.latest_id().generation;

  WorldCommand command;
  command.entity = "entity/ghost";
  command.delta_velocity_m_s = {1.0, 0.0, 0.0};
  world.push_command(command);

  CHECK_FALSE(world.step(1.0, &error));
  CHECK(error.find("unknown entity") != std::string::npos);
  CHECK(world.step_index() == 1u);
  CHECK(world.latest_id().generation == generation);
  CHECK(world.find("entity/a")->position_m[0] == 1.0);
}

TEST_CASE("a non-finite state blocks the commit", "[world]") {
  World world;
  std::string error;
  WorldEntity entity = make_entity("entity/a", 0.0, 0.0);
  entity.velocity_m_s[0] = std::numeric_limits<double>::infinity();
  REQUIRE(world.add_entity(entity, &error));

  CHECK_FALSE(world.step(1.0, &error));
  CHECK(error.find("non-finite") != std::string::npos);
  CHECK(world.step_index() == 0u);
  CHECK_FALSE(world.acquire_latest().has_value());
}

TEST_CASE("an invalid step length is rejected", "[world]") {
  World world;
  std::string error;
  REQUIRE(world.add_entity(make_entity("entity/a", 0.0, 1.0), &error));
  CHECK_FALSE(world.step(-1.0, &error));
  CHECK(world.step_index() == 0u);
  CHECK_FALSE(world.step(std::numeric_limits<double>::quiet_NaN(), &error));
}

TEST_CASE("old generations are evicted but the newest stays readable", "[world]") {
  World world(2);
  std::string error;
  REQUIRE(world.add_entity(make_entity("entity/a", 0.0, 1.0), &error));
  REQUIRE(world.step(1.0, &error));
  const auto first = world.latest_id();
  REQUIRE(world.step(1.0, &error));
  REQUIRE(world.step(1.0, &error));
  REQUIRE(world.step(1.0, &error));

  const auto newest = world.acquire_latest();
  REQUIRE(newest.has_value());
  // The first generation has been evicted (retained = 2).
  CHECK_FALSE(world.acquire(first).has_value());
  CHECK(world.acquire_latest()->bytes().size() ==
        World::kSnapshotHeaderBytes + World::kBytesPerEntity);
  CHECK(world.latest_id().generation > first.generation);
}
TEST_CASE("an environment turns the world into an orbit integrator", "[world]") {
  World world;
  std::string error;
  constexpr double mu = 3.986004418e14;
  constexpr double radius = 7.0e6;
  WorldEntity orbiter;
  orbiter.id = "entity/orbiter";
  orbiter.position_m = {radius, 0.0, 0.0};
  orbiter.velocity_m_s = {0.0, srsp::core::circular_speed(mu, radius), 0.0};
  REQUIRE(world.add_entity(orbiter, &error));

  srsp::core::EnvironmentSpec spec;
  spec.earth.id = "earth";
  spec.earth.mu = mu;
  spec.earth.radius_m = 6.371e6;
  world.set_environment(spec);
  REQUIRE(world.has_environment());

  const double period = srsp::core::orbital_period(mu, radius);
  const int steps = 2000;
  const double dt = period / static_cast<double>(steps);
  for (int i = 0; i < steps; ++i) {
    REQUIRE(world.step(dt, &error));
  }
  const auto* after = world.find("entity/orbiter");
  REQUIRE(after != nullptr);
  const double drift =
      srsp::core::norm({after->position_m[0] - radius, after->position_m[1], after->position_m[2]});
  std::printf("world orbit drift=%.3e m\n", drift);
  CHECK(drift / radius < 1e-6);
  CHECK(world.latest_id().generation == static_cast<std::uint64_t>(steps));
}
TEST_CASE("thrust accelerates and burns propellant", "[world]") {
  World world;
  std::string error;
  WorldEntity tug;
  tug.id = "entity/tug";
  tug.mass_kg = 1000.0;
  tug.propellant_kg = 100.0;
  tug.isp_s = 300.0;
  tug.thrust_n = 100.0;
  REQUIRE(world.add_entity(tug, &error));

  const double dt = 0.01;
  const int steps = 100;
  for (int i = 0; i < steps; ++i) {
    REQUIRE(world.step(dt, &error));
  }
  const auto* after = world.find("entity/tug");
  REQUIRE(after != nullptr);
  const double m_dot = 100.0 / (300.0 * 9.80665);
  const double expected_mass = 1000.0 - m_dot * (dt * steps);
  CHECK(std::abs(after->mass_kg - expected_mass) < 1e-6);
  CHECK(std::abs(after->propellant_kg - (100.0 - m_dot * dt * steps)) < 1e-6);
  // Small-dt approximation: v ~= F/m0 * t = 0.1 m/s after one second.
  CHECK(after->velocity_m_s[0] > 0.09);
  CHECK(after->position_m[0] > 0.0);
}

TEST_CASE("propellant exhaustion stops the thrust", "[world]") {
  World world;
  std::string error;
  WorldEntity tug;
  tug.id = "entity/tug";
  tug.mass_kg = 100.0;
  tug.propellant_kg = 0.005;
  tug.isp_s = 300.0;
  tug.thrust_n = 100.0;
  REQUIRE(world.add_entity(tug, &error));

  for (int i = 0; i < 100; ++i) {
    REQUIRE(world.step(0.01, &error));
  }
  const auto* after = world.find("entity/tug");
  REQUIRE(after != nullptr);
  CHECK(after->propellant_kg == 0.0);
  CHECK(after->thrust_n == 0.0);
  CHECK(std::abs(after->mass_kg - 99.995) < 1e-6);

  const double velocity = after->velocity_m_s[0];
  REQUIRE(world.step(0.01, &error));
  CHECK(world.find("entity/tug")->velocity_m_s[0] == velocity);
}

TEST_CASE("a thrust command sets the actuator", "[world]") {
  World world;
  std::string error;
  WorldEntity actuator = make_entity("entity/tug", 0.0, 0.0);
  actuator.isp_s = 300.0;
  actuator.propellant_kg = 1.0;
  REQUIRE(world.add_entity(actuator, &error));
  WorldCommand command;
  command.entity = "entity/tug";
  command.kind = srsp::core::WorldCommandKind::kSetThrust;
  command.value = 25.0;
  world.push_command(command);
  REQUIRE(world.step(0.01, &error));
  CHECK(world.find("entity/tug")->thrust_n == 25.0);
}

TEST_CASE("thrust without an impulse is rejected", "[world]") {
  World world;
  std::string error;
  WorldEntity tug = make_entity("entity/tug", 0.0, 0.0);
  tug.thrust_n = 10.0;
  tug.isp_s = 0.0;
  REQUIRE(world.add_entity(tug, &error));
  CHECK_FALSE(world.step(0.01, &error));
  CHECK(error.find("isp") != std::string::npos);
  CHECK(world.step_index() == 0u);
}
namespace {

double decode_mass(const SnapshotStore::Lease& lease, std::size_t index) {
  double value = 0.0;
  const std::size_t offset =
      World::kSnapshotHeaderBytes + index * World::kBytesPerEntity + 64 + 6 * sizeof(double);
  std::memcpy(&value, lease.bytes().data() + offset, sizeof(double));
  return value;
}

srsp::core::EnvironmentSpec sun_and_earth() {
  srsp::core::EnvironmentSpec spec;
  spec.earth.id = "earth";
  spec.earth.mu = 3.986004418e14;
  spec.earth.radius_m = 6.371e6;
  spec.sun.id = "sun";
  spec.sun.position_m = {1.495978707e11, 0.0, 0.0};
  return spec;
}

}  // namespace

TEST_CASE("solar pressure pushes the entity away from the sun", "[world]") {
  World world;
  std::string error;
  WorldEntity craft;
  craft.id = "entity/craft";
  craft.mass_kg = 100.0;
  craft.area_m2 = 2.0;
  craft.reflectivity = 1.5;
  craft.position_m = {7.0e6, 0.0, 0.0};
  craft.velocity_m_s = {0.0, srsp::core::circular_speed(3.986004418e14, 7.0e6), 0.0};
  REQUIRE(world.add_entity(craft, &error));
  world.set_environment(sun_and_earth());

  for (int i = 0; i < 100; ++i) {
    REQUIRE(world.step(1.0, &error));
  }
  const auto* after = world.find("entity/craft");
  REQUIRE(after != nullptr);
  // The sun is at +x, so the pressure accelerates along -x.
  CHECK(after->velocity_m_s[0] < 0.0);
}

TEST_CASE("atmospheric drag removes orbital energy", "[world]") {
  World world;
  std::string error;
  WorldEntity craft;
  craft.id = "entity/craft";
  craft.mass_kg = 500.0;
  craft.area_m2 = 4.0;
  craft.drag_coefficient = 2.2;
  craft.position_m = {6.771e6, 0.0, 0.0};  // 400 km altitude
  craft.velocity_m_s = {0.0, srsp::core::circular_speed(3.986004418e14, 6.771e6), 0.0};
  REQUIRE(world.add_entity(craft, &error));

  srsp::core::EnvironmentSpec spec = sun_and_earth();
  spec.atmosphere.enabled = true;
  spec.atmosphere.rho0_kg_m3 = 4.0e-12;
  spec.atmosphere.reference_altitude_m = 400.0e3;
  spec.atmosphere.scale_height_m = 60.0e3;
  world.set_environment(spec);

  const double before = srsp::core::norm(world.find("entity/craft")->velocity_m_s);
  for (int i = 0; i < 200; ++i) {
    REQUIRE(world.step(1.0, &error));
  }
  const double after = srsp::core::norm(world.find("entity/craft")->velocity_m_s);
  CHECK(after < before);
}

TEST_CASE("the snapshot carries the entity mass", "[world]") {
  World world;
  std::string error;
  WorldEntity craft = make_entity("entity/craft", 0.0, 0.0);
  craft.mass_kg = 250.0;
  REQUIRE(world.add_entity(craft, &error));
  REQUIRE(world.step(0.1, &error));
  const auto lease = world.acquire_latest();
  REQUIRE(lease.has_value());
  CHECK(lease->bytes().size() == World::kSnapshotHeaderBytes + World::kBytesPerEntity);
  CHECK(decode_mass(*lease, 0u) == 250.0);
}

TEST_CASE("pressure-centre offsets produce a torque diagnostic", "[world]") {
  World world;
  std::string error;
  WorldEntity craft;
  craft.id = "entity/craft";
  craft.mass_kg = 100.0;
  craft.area_m2 = 2.0;
  craft.reflectivity = 1.5;
  craft.center_of_pressure_m = {0.0, 0.0, 1.0};
  craft.position_m = {7.0e6, 0.0, 0.0};
  craft.velocity_m_s = {0.0, srsp::core::circular_speed(3.986004418e14, 7.0e6), 0.0};
  REQUIRE(world.add_entity(craft, &error));
  world.set_environment(sun_and_earth());
  REQUIRE(world.step(1.0, &error));
  const auto* after = world.find("entity/craft");
  REQUIRE(after != nullptr);
  // Force along -x crossed with a +z offset gives a torque on -y.
  CHECK(std::abs(after->last_torque_nm[1]) > 0.0);
}
TEST_CASE("gravity-gradient torque spins a tilted body", "[world]") {
  World world;
  std::string error;
  WorldEntity craft;
  craft.id = "entity/craft";
  craft.mass_kg = 100.0;
  craft.inertia = {10.0, 0.0, 0.0, 0.0, 20.0, 0.0, 0.0, 0.0, 30.0};
  craft.position_m = {7.0e6, 0.0, 0.0};
  craft.velocity_m_s = {0.0, srsp::core::circular_speed(3.986004418e14, 7.0e6), 0.0};
  // Tilt about z so the principal axes are no longer aligned with r.
  craft.attitude = {0.0, 0.0, 0.3826834, 0.9238795};
  REQUIRE(world.add_entity(craft, &error));
  world.set_environment(sun_and_earth());

  for (int i = 0; i < 100; ++i) {
    REQUIRE(world.step(0.1, &error));
  }
  const auto* after = world.find("entity/craft");
  REQUIRE(after != nullptr);
  CHECK(srsp::core::norm(after->omega_body_rad_s) > 1e-9);

  double quat_norm = 0.0;
  for (double value : after->attitude) {
    quat_norm += value * value;
  }
  CHECK(std::abs(std::sqrt(quat_norm) - 1.0) < 1e-12);
}

TEST_CASE("a saturated wheel stops absorbing momentum", "[world]") {
  World world;
  std::string error;
  WorldEntity craft;
  craft.id = "entity/craft";
  craft.mass_kg = 100.0;
  craft.inertia = {10.0, 0.0, 0.0, 0.0, 10.0, 0.0, 0.0, 0.0, 10.0};
  craft.wheel_max_momentum_nms = 0.05;
  craft.position_m = {7.0e6, 0.0, 0.0};
  craft.velocity_m_s = {0.0, srsp::core::circular_speed(3.986004418e14, 7.0e6), 0.0};
  REQUIRE(world.add_entity(craft, &error));
  world.set_environment(sun_and_earth());

  WorldCommand command;
  command.entity = "entity/craft";
  command.kind = srsp::core::WorldCommandKind::kSetWheelTorque;
  command.value = 1.0;  // N*m, far above what the wheel can store
  world.push_command(command);

  for (int i = 0; i < 5; ++i) {
    REQUIRE(world.step(0.1, &error));
  }
  const auto* after = world.find("entity/craft");
  REQUIRE(after != nullptr);
  CHECK(std::abs(after->wheel_momentum_nms - 0.05) < 1e-12);
  // The reaction torque turns the body the other way.
  CHECK(after->omega_body_rad_s[0] < 0.0);
}
TEST_CASE("mass properties follow the propellant", "[world]") {
  World world;
  std::string error;
  WorldEntity craft;
  craft.id = "entity/craft";
  craft.dry_mass_kg = 90.0;
  craft.dry_com_m = {0.1, 0.0, 0.0};
  craft.dry_inertia = {10.0, 0.0, 0.0, 0.0, 20.0, 0.0, 0.0, 0.0, 30.0};
  craft.propellant_kg = 10.0;
  craft.tank_com_m = {0.0, 0.0, 1.0};
  REQUIRE(world.add_entity(craft, &error));
  world.set_environment(sun_and_earth());
  REQUIRE(world.step(1.0, &error));

  const auto* after = world.find("entity/craft");
  REQUIRE(after != nullptr);
  CHECK(after->mass_kg == 100.0);
  CHECK(std::abs(after->center_of_mass_m[0] - 0.09) < 1e-12);
  CHECK(std::abs(after->center_of_mass_m[1]) < 1e-12);
  CHECK(std::abs(after->center_of_mass_m[2] - 0.1) < 1e-12);
  // Parallel-axis terms move the tensor away from the dry one.
  CHECK(after->inertia[0] > 10.0);
  CHECK(after->inertia[8] > 30.0);
}

TEST_CASE("the snapshot carries the attitude", "[world]") {
  World world;
  std::string error;
  WorldEntity craft = make_entity("entity/craft", 7.0e6, 0.0);
  craft.attitude = {0.0, 0.0, 0.3826834, 0.9238795};
  REQUIRE(world.add_entity(craft, &error));
  world.set_environment(sun_and_earth());
  REQUIRE(world.step(0.1, &error));

  const auto lease = world.acquire_latest();
  REQUIRE(lease.has_value());
  const std::size_t base = World::kSnapshotHeaderBytes + 64 + 7 * sizeof(double);
  double quaternion[4] = {0.0, 0.0, 0.0, 0.0};
  std::memcpy(quaternion, lease->bytes().data() + base, sizeof(quaternion));
  const auto* after = world.find("entity/craft");
  REQUIRE(after != nullptr);
  CHECK(std::abs(quaternion[3] - after->attitude[3]) < 1e-12);
  CHECK(std::abs(quaternion[2] - after->attitude[2]) < 1e-12);
}
TEST_CASE("the snapshot header records version, stride and count", "[world]") {
  World world;
  std::string error;
  REQUIRE(world.add_entity(make_entity("entity/a", 0.0, 0.0), &error));
  REQUIRE(world.add_entity(make_entity("entity/b", 0.0, 0.0), &error));
  REQUIRE(world.step(0.1, &error));
  const auto lease = world.acquire_latest();
  REQUIRE(lease.has_value());
  const auto header = World::decode_header(lease->bytes());
  CHECK(header.version == World::kSnapshotVersion);
  CHECK(header.stride == World::kBytesPerEntity);
  CHECK(header.count == 2u);
}

TEST_CASE("thrusters desaturate a full wheel", "[world]") {
  World world;
  std::string error;
  WorldEntity craft;
  craft.id = "entity/craft";
  craft.mass_kg = 100.0;
  craft.propellant_kg = 1.0;
  craft.isp_s = 300.0;
  craft.inertia = {10.0, 0.0, 0.0, 0.0, 10.0, 0.0, 0.0, 0.0, 10.0};
  craft.wheel_max_momentum_nms = 0.05;
  craft.wheel_momentum_nms = 0.05;  // already full
  craft.desaturation_torque_nm = 0.01;
  craft.position_m = {7.0e6, 0.0, 0.0};
  craft.velocity_m_s = {0.0, srsp::core::circular_speed(3.986004418e14, 7.0e6), 0.0};
  REQUIRE(world.add_entity(craft, &error));
  world.set_environment(sun_and_earth());

  REQUIRE(world.step(0.1, &error));
  const auto* after = world.find("entity/craft");
  REQUIRE(after != nullptr);
  CHECK(after->wheel_momentum_nms < 0.05);
  CHECK(after->propellant_kg < 1.0);
}

TEST_CASE("the pressure centre moves with the angle of attack", "[world]") {
  World world;
  std::string error;
  WorldEntity craft;
  craft.id = "entity/craft";
  craft.mass_kg = 100.0;
  craft.area_m2 = 2.0;
  craft.drag_coefficient = 2.2;
  craft.aero_cp_arm_m = 0.5;
  craft.position_m = {6.771e6, 0.0, 0.0};
  // Velocity with a vertical component: the flow hits at an angle.
  craft.velocity_m_s = {0.0, 7500.0, 500.0};
  REQUIRE(world.add_entity(craft, &error));

  srsp::core::EnvironmentSpec spec = sun_and_earth();
  spec.atmosphere.enabled = true;
  spec.atmosphere.rho0_kg_m3 = 4.0e-12;
  spec.atmosphere.reference_altitude_m = 400.0e3;
  spec.atmosphere.scale_height_m = 60.0e3;
  world.set_environment(spec);
  REQUIRE(world.step(0.1, &error));

  const auto* after = world.find("entity/craft");
  REQUIRE(after != nullptr);
  // An angle of attack produces a pitching torque about body Y.
  CHECK(std::abs(after->last_torque_nm[1]) > 0.0);
}