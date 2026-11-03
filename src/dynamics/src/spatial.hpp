/**
 * @file spatial.hpp
 * @brief Spatial algebra primitives for the O(n) solver (ABA).
 *
 * Vectors are ordered [linear; angular] to match the Jacobian layout used by
 * RigidBodyDynamics (top three rows linear, bottom three angular). All
 * quantities are expressed in the world frame about the body origin.
 */
#ifndef SRSP_DYNAMICS_SPATIAL_HPP
#define SRSP_DYNAMICS_SPATIAL_HPP

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace srsp::dynamics {

/** @brief 6x6 spatial inertia about the body origin, world frame. */
/** @brief_zh 体原点、世界系下的 6×6 空间惯量。 */
using SpatialInertia = Eigen::Matrix<double, 6, 6>;

/** @brief 6-vector ordered [linear; angular]. */
/** @brief_zh 6 维向量，顺序为 [线速度; 角速度]。 */
using SpatialVector = Eigen::Matrix<double, 6, 1>;

/** @brief Skew-symmetric cross-product matrix of a 3-vector. */
/** @brief_zh 三维向量的反对称叉乘矩阵。 */
inline Eigen::Matrix3d skew(const Eigen::Vector3d& v) {
  Eigen::Matrix3d m;
  m << 0.0, -v.z(), v.y(), v.z(), 0.0, -v.x(), -v.y(), v.x(), 0.0;
  return m;
}

/**
 * @brief Builds the spatial inertia of a rigid body about its origin.
 * @param mass Mass in kg.
 * @param com_world Vector from the body origin to the centre of mass, world.
 * @param inertia_world Rotational inertia about the centre of mass, world.
 *
 * The result satisfies `0.5 * v^T I v` equals the kinetic energy for any
 * spatial velocity `v = [linear; angular]`.
 */
[[nodiscard]] inline SpatialInertia make_spatial_inertia(double mass,
                                                         const Eigen::Vector3d& com_world,
                                                         const Eigen::Matrix3d& inertia_world) {
  SpatialInertia inertia = SpatialInertia::Zero();
  const Eigen::Matrix3d c = skew(com_world);
  inertia.topLeftCorner<3, 3>() = mass * Eigen::Matrix3d::Identity();
  inertia.topRightCorner<3, 3>() = -mass * c;
  inertia.bottomLeftCorner<3, 3>() = mass * c;
  inertia.bottomRightCorner<3, 3>() = inertia_world - mass * c * c;
  return inertia;
}

/** @brief Kinetic energy for a spatial velocity and inertia. */
[[nodiscard]] inline double kinetic_energy(const SpatialInertia& inertia,
                                           const SpatialVector& velocity) {
  return 0.5 * velocity.dot(inertia * velocity);
}

/**
 * @brief Spatial force bias `v x* (I v)` used by the ABA backward sweep.
 *
 * The motion cross product is applied to the momentum; the result is the
 * force that keeps the body's momentum consistent with its velocity.
 */
[[nodiscard]] inline SpatialVector bias_force(const SpatialVector& velocity,
                                              const SpatialVector& momentum) {
  const Eigen::Vector3d linear = velocity.head<3>();
  const Eigen::Vector3d angular = velocity.tail<3>();
  const Eigen::Vector3d force = momentum.head<3>();
  const Eigen::Vector3d torque = momentum.tail<3>();
  SpatialVector out;
  out.head<3>() = angular.cross(force);
  out.tail<3>() = angular.cross(torque) + linear.cross(force);
  return out;
}

/**
 * @brief Spatial acceleration bias `v x (v)` (the `c` term of ABA).
 *
 * Zero for pure rotation about the origin; used to propagate accelerations.
 */
[[nodiscard]] inline SpatialVector bias_acceleration(const SpatialVector& velocity) {
  const Eigen::Vector3d linear = velocity.head<3>();
  const Eigen::Vector3d angular = velocity.tail<3>();
  SpatialVector out;
  out.head<3>() = angular.cross(linear);
  out.tail<3>() = Eigen::Vector3d::Zero();
  return out;
}

}  // namespace srsp::dynamics

#endif  // SRSP_DYNAMICS_SPATIAL_HPP
