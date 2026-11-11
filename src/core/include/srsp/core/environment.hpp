/**
 * @file environment.hpp
 * @brief W4: celestial environment sampling and two-body orbit propagation.
 *
 * One call returns everything the physics and rendering layers need for a given
 * entity position and time: relative sun direction, solar flux, eclipse factor
 * and the accelerations contributed by the central body, third bodies and J2.
 * Rendering uses the same sample, so light and solar pressure never disagree.
 */
#ifndef SRSP_CORE_ENVIRONMENT_HPP
#define SRSP_CORE_ENVIRONMENT_HPP

#include <array>
#include <string>

#include <cstddef>

namespace srsp::core {

using Vec3 = std::array<double, 3>;

/** @brief A celestial body: gravity parameter, radius, position and shape terms. */
struct CelestialBody {
  std::string id;                   ///< "sun", "earth", "moon".
  double mu = 0.0;                  ///< Gravitational parameter, m^3/s^2.
  double radius_m = 0.0;            ///< Mean radius, m.
  double j2 = 0.0;                  ///< Zonal coefficient (0 disables the term).
  double reference_radius_m = 0.0;  ///< Radius used by J2.
  Vec3 position_m{};                ///< Inertial position at t = 0 (fixed during a run).
};

/** @brief Exponential atmosphere (disabled unless enabled is true). */
struct AtmosphereSpec {
  bool enabled = false;
  double rho0_kg_m3 = 0.0;  ///< Density at the reference altitude.
  double reference_altitude_m = 0.0;
  double scale_height_m = 8500.0;  ///< Density scale height.
};

/** @brief Environment inputs, one instance per world. */
struct EnvironmentSpec {
  CelestialBody sun;
  CelestialBody earth;
  CelestialBody moon;
  bool use_third_body = false;      ///< Adds moon/sun tides when true.
  bool use_j2 = false;              ///< Adds the J2 term when true.
  double solar_flux_w_m2 = 1361.0;  ///< Flux at 1 AU.
  AtmosphereSpec atmosphere;        ///< Optional drag model.
};

/** @brief Everything derived for one entity at one instant. */
struct EnvironmentSample {
  Vec3 sun_direction{1.0, 0.0, 0.0};  ///< Unit vector from the entity to the sun.
  Vec3 gravity_m_s2{};                ///< Central + third-body + J2 acceleration.
  double solar_flux_w_m2 = 0.0;       ///< Flux at the entity, before eclipse.
  double visibility = 1.0;            ///< 1 lit, 0 umbra.
  double density_kg_m3 = 0.0;         ///< Atmospheric density at the entity.
  bool eclipse = false;               ///< True when visibility == 0.
};

/**
 * @brief Samples the environment for @p entity_position.
 * @param spec Celestial configuration.
 * @param entity_position Inertial position, metres.
 * @return Sun direction, flux, visibility and gravitational acceleration.
 */
[[nodiscard]] EnvironmentSample evaluate_environment(const EnvironmentSpec& spec,
                                                     const Vec3& entity_position);

/** @brief Two-body state used by the propagator. */
struct OrbitState {
  Vec3 position_m{};
  Vec3 velocity_m_s{};
};

/**
 * @brief Advances a two-body state by @p dt with classical RK4.
 * @param central Central body (mu, radius, j2 terms honoured).
 * @param spec Full environment, used for third-body acceleration.
 * @param state State to advance in place.
 * @param dt Step length, seconds.
 * @return False when the step length or the resulting state is not finite.
 */
bool propagate_orbit(const EnvironmentSpec& spec, OrbitState* state, double dt);

/** @brief Circular orbital speed for radius @p r and parameter @p mu. */
[[nodiscard]] double circular_speed(double mu, double r);

/** @brief Orbital period for radius @p r and parameter @p mu. */
[[nodiscard]] double orbital_period(double mu, double r);

/** @brief Parses a celestial component manifest (kind "env.celestial"). */
/**
 * @param json_text Manifest contents.
 * @param body Receives mu/radius/j2/position.
 * @param error Receives the first problem.
 * @return False when the kind is wrong or a required parameter is missing.
 */
[[nodiscard]] bool celestial_from_manifest(const std::string& json_text, CelestialBody* body,
                                           std::string* error);

/**
 * @brief Solar radiation pressure acceleration for one entity.
 * @param sample Environment sample (flux and visibility are used).
 * @param area_m2 Cross-sectional area.
 * @param mass_kg Entity mass.
 * @param reflectivity Reflectivity coefficient Cr (1 is black body, 2 mirror).
 * @return Acceleration along the anti-sun direction, m/s^2.
 */
[[nodiscard]] Vec3 solar_radiation_pressure_acceleration(const EnvironmentSample& sample,
                                                         double area_m2, double mass_kg,
                                                         double reflectivity);

/**
 * @brief Gravity-gradient torque about the entity origin.
 * @param mu Central body parameter.
 * @param position Inertial position.
 * @param inertia Row-major inertia tensor about the centre of mass.
 * @return Torque vector, N*m.
 */
[[nodiscard]] Vec3 gravity_gradient_torque(double mu, const Vec3& position,
                                           const std::array<double, 9>& inertia);

/** @brief Torque produced by a force applied at a centre-of-pressure offset. */
[[nodiscard]] Vec3 torque_from_force(const Vec3& force, const Vec3& offset_m);

/**
 * @brief Atmospheric drag acceleration (exponential density, no co-rotation).
 * @param spec Environment with atmosphere.enabled set.
 * @param position Inertial position, m.
 * @param velocity Inertial velocity, m/s.
 * @param area_m2 Cross-sectional area.
 * @param mass_kg Entity mass.
 * @param drag_coefficient Cd (typically 2.2).
 * @return Acceleration opposing the velocity, m/s^2.
 */
[[nodiscard]] Vec3 atmospheric_drag_acceleration(const EnvironmentSpec& spec, const Vec3& position,
                                                 const Vec3& velocity, double area_m2,
                                                 double mass_kg, double drag_coefficient);
/** @brief Norm of a vector. */
[[nodiscard]] double norm(const Vec3& value);

}  // namespace srsp::core

#endif  // SRSP_CORE_ENVIRONMENT_HPP
