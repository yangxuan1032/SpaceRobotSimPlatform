/**
 * @file force_models.hpp
 * @brief Environmental force models: two-body gravity, third body, SRP, drag.
 * @brief_zh 环境力模型：二体引力、第三体引力、太阳光压、大气阻力。
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
/** @brief_zh 力模型使用的物理常数（SI 单位）。 */
struct ForceConstants {
  static constexpr double kAstronomicalUnit = 1.495978707e11;  ///< m / 天文单位（米）。
  static constexpr double kSunMu = 1.32712440018e20;           ///< m^3/s^2 / 太阳引力常数。
  static constexpr double kSolarFlux = 1361.0;          ///< W/m^2 at 1 AU / 1 AU 处太阳常数。
  static constexpr double kSpeedOfLight = 299792458.0;  ///< m/s / 光速。
  static constexpr double kEarthRadius = 6.371e6;       ///< m / 地球平均半径（米）。
  static constexpr double kEarthMu = 3.986004418e14;    ///< m^3/s^2 / 地球引力常数。
};

/** @brief Atmosphere model parameters (exponential density). */
/** @brief_zh 大气模型参数（指数密度）。 */
struct Atmosphere {
  double density_at_reference =
      0.0;  ///< kg/m^3 at the reference altitude. / 参考高度处密度（kg/m³）。
  double reference_altitude = 400.0e3;  ///< m above the surface. / 参考高度（m，相对地表）。
  double scale_height = 8.5e3;          ///< m. / 标高（米）。
  double drag_coefficient = 2.2;        ///< Dimensionless. / 阻力系数（无量纲）。
  double area = 1.0;                    ///< m^2. / 迎风面积（m²）。

  /** @brief Density at @p altitude (m above the mean radius), kg/m^3. */
  [[nodiscard]] double density(double altitude) const;
};

/** @brief Solar radiation pressure parameters. */
/** @brief_zh 太阳光压参数。 */
struct SolarPressure {
  double area = 1.0;  ///< m^2. / 受照面积（m²）。
  double reflectivity =
      1.0;  ///< 0 = absorb, 1 = specular, 2 = retro. / 反射率：0 吸收、1 镜面、2 逆反射。
  double flux = ForceConstants::kSolarFlux;  ///< W/m^2 at the body. / 体所在处太阳常数（W/m²）。
  bool in_eclipse = false;  ///< True inside umbra: no force. / 位于本影时为真，此时无光压。
};

/** @brief Point-mass gravity of a central body. */
/** @brief_zh 中心天体的点质量引力。 */
Eigen::Vector3d two_body_gravity(double mu, double mass, const Eigen::Vector3d& position);

/** @brief Point-mass gravity of a third body (Sun, Moon, planet). */
/** @brief_zh 第三体（太阳、月球、行星）的点质量引力。 */
Eigen::Vector3d third_body(double mu_third, double mass, const Eigen::Vector3d& body_position,
                           const Eigen::Vector3d& third_body_position);

/**
 * @brief Solar radiation pressure force.
 * @brief_zh 太阳光压力。
 * @param sun_direction Unit vector from the body towards the Sun.
 */
Eigen::Vector3d solar_radiation_pressure(const SolarPressure& pressure,
                                         const Eigen::Vector3d& sun_direction);

/**
 * @brief Atmospheric drag force.
 * @brief_zh 大气阻力。
 * @param velocity World-frame velocity of the body, m/s.
 */
Eigen::Vector3d atmospheric_drag(const Atmosphere& atmosphere, double mass,
                                 const Eigen::Vector3d& position, const Eigen::Vector3d& velocity);

}  // namespace srsp::dynamics

#endif  // SRSP_DYNAMICS_FORCE_MODELS_HPP
