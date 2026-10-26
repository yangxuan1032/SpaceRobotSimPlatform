/**
 * @file force_models.hpp
 * @brief Environmental force models: two-body gravity, third body, SRP, drag.
 *
 * All functions return a world-frame force in newtons for one body of the given
 * mass. They are pure functions of the state and the environment, so they can be
 * evaluated inside every RK4 stage by the dynamics integrator.
 */
#ifndef SRSP_DYNAMICS_FORCE_MODELS_HPP
#define SRSP_DYNAMICS_FORCE_MODELS_HPP

#include <Eigen/Dense>

namespace srsp::dynamics {

/** @brief Physical constants used by the force models (SI units). */
struct ForceConstants {
  static constexpr double kAstronomicalUnit = 1.495978707e11;  ///< m
  static constexpr double kSunMu = 1.32712440018e20;           ///< m^3/s^2
  static constexpr double kSolarFlux = 1361.0;                 ///< W/m^2 at 1 AU
  static constexpr double kSpeedOfLight = 299792458.0;         ///< m/s
  static constexpr double kEarthRadius = 6.371e6;              ///< m
  static constexpr double kEarthMu = 3.986004418e14;           ///< m^3/s^2
};

/** @brief Atmosphere model parameters (exponential density). */
struct Atmosphere {
  double density_at_reference = 0.0;    ///< kg/m^3 at the reference altitude.
  double reference_altitude = 400.0e3;  ///< m above the surface.
  double scale_height = 8.5e3;          ///< m.
  double drag_coefficient = 2.2;        ///< Dimensionless.
  double area = 1.0;                    ///< m^2.

  /** @brief Density at @p altitude (m above the mean radius), kg/m^3. */
  [[nodiscard]] double density(double altitude) const;
};

/** @brief Solar radiation pressure parameters. */
struct SolarPressure {
  double area = 1.0;                         ///< m^2.
  double reflectivity = 1.0;                 ///< 0 = absorb, 1 = specular, 2 = retro.
  double flux = ForceConstants::kSolarFlux;  ///< W/m^2 at the body.
  bool in_eclipse = false;                   ///< True inside umbra: no force.
};

/** @brief Point-mass gravity of a central body. */
Eigen::Vector3d two_body_gravity(double mu, double mass, const Eigen::Vector3d& position);

/** @brief Point-mass gravity of a third body (Sun, Moon, planet). */
Eigen::Vector3d third_body(double mu_third, double mass, const Eigen::Vector3d& body_position,
                           const Eigen::Vector3d& third_body_position);

/**
 * @brief Solar radiation pressure force.
 * @param sun_direction Unit vector from the body towards the Sun.
 */
Eigen::Vector3d solar_radiation_pressure(const SolarPressure& pressure,
                                         const Eigen::Vector3d& sun_direction);

/**
 * @brief Atmospheric drag force.
 * @param velocity World-frame velocity of the body, m/s.
 */
Eigen::Vector3d atmospheric_drag(const Atmosphere& atmosphere, double mass,
                                 const Eigen::Vector3d& position, const Eigen::Vector3d& velocity);

}  // namespace srsp::dynamics

#endif  // SRSP_DYNAMICS_FORCE_MODELS_HPP
