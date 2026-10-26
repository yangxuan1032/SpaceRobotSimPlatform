/**
 * @file force_models.cpp
 * @brief Implementations of the environmental force models.
 */
#include <srsp/dynamics/force_models.hpp>

#include <cmath>

namespace srsp::dynamics {

double Atmosphere::density(double altitude) const {
  if (density_at_reference <= 0.0 || scale_height <= 0.0) {
    return 0.0;
  }
  return density_at_reference * std::exp(-(altitude - reference_altitude) / scale_height);
}

Eigen::Vector3d two_body_gravity(double mu, double mass, const Eigen::Vector3d& position) {
  const double distance = position.norm();
  if (distance <= 0.0) {
    return Eigen::Vector3d::Zero();
  }
  return -mu * mass * position / (distance * distance * distance);
}

Eigen::Vector3d third_body(double mu_third, double mass, const Eigen::Vector3d& body_position,
                           const Eigen::Vector3d& third_body_position) {
  const Eigen::Vector3d to_third = third_body_position - body_position;
  const double distance = to_third.norm();
  if (distance <= 0.0) {
    return Eigen::Vector3d::Zero();
  }
  return mu_third * mass * to_third / (distance * distance * distance);
}

Eigen::Vector3d solar_radiation_pressure(const SolarPressure& pressure,
                                         const Eigen::Vector3d& sun_direction) {
  if (pressure.in_eclipse) {
    return Eigen::Vector3d::Zero();
  }
  const double direction_norm = sun_direction.norm();
  if (direction_norm <= 0.0) {
    return Eigen::Vector3d::Zero();
  }
  const Eigen::Vector3d unit = sun_direction / direction_norm;
  // P = F / c; force on a flat plate: P * A * (1 + rho) for specular reflection.
  const double radiation_pressure = pressure.flux / ForceConstants::kSpeedOfLight;
  const double magnitude = radiation_pressure * pressure.area * (1.0 + pressure.reflectivity);
  return unit * magnitude;
}

Eigen::Vector3d atmospheric_drag(const Atmosphere& atmosphere, double mass,
                                 const Eigen::Vector3d& position, const Eigen::Vector3d& velocity) {
  (void)mass;  // Drag depends on area, not mass; kept for API symmetry.
  const double speed = velocity.norm();
  if (speed <= 0.0) {
    return Eigen::Vector3d::Zero();
  }
  const double altitude = position.norm() - ForceConstants::kEarthRadius;
  const double density = atmosphere.density(altitude);
  if (density <= 0.0) {
    return Eigen::Vector3d::Zero();
  }
  const double magnitude =
      0.5 * density * speed * speed * atmosphere.drag_coefficient * atmosphere.area;
  return -velocity / speed * magnitude;
}

}  // namespace srsp::dynamics
