/**
 * @file rigid_body.hpp
 * @brief Floating-base rigid-body dynamics over an SRM model.
 *
 * Generalised coordinates (nq = 7 + joints):
 *   [0..2]  base position, world frame, m
 *   [3..6]  base orientation quaternion (x, y, z, w), body -> world
 *   [7..]   joint positions (rad or m)
 *
 * Generalised velocities (nv = 6 + joints):
 *   [0..2]  linear velocity of the base origin, world frame, m/s
 *   [3..5]  angular velocity, world frame, rad/s
 *   [6..]   joint rates
 *
 * Equations of motion: M(q) * dv/dt + h(q, v) = tau, with h = C(q, v) v - Q_g.
 * The Coriolis term is currently evaluated by central differences of M (exact
 * for the mass matrix, approximate for the bias); replacing it with RNEA is
 * tracked as M1.4b.
 */
#ifndef SRSP_DYNAMICS_RIGID_BODY_HPP
#define SRSP_DYNAMICS_RIGID_BODY_HPP

#include <srsp/model/srm.hpp>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <Eigen/Dense>

namespace srsp::dynamics {

/** @brief Pose of a body in the world frame. */
struct Pose {
  Eigen::Vector3d position{0.0, 0.0, 0.0};                ///< Body origin, m.
  Eigen::Matrix3d rotation{Eigen::Matrix3d::Identity()};  ///< Body -> world.
};

/** @brief Floating-base rigid-body model built from an SRM description. */
class RigidBodyDynamics {
 public:
  /**
   * @brief Compiles the kinematic tree and inertia properties.
   * @param model Validated SRM model.
   */
  explicit RigidBodyDynamics(const srsp::model::Model& model);

  [[nodiscard]] int nq() const noexcept { return nq_; }
  [[nodiscard]] int nv() const noexcept { return nv_; }
  [[nodiscard]] int joint_count() const noexcept { return joint_count_; }
  [[nodiscard]] const std::vector<std::string>& body_ids() const noexcept { return body_ids_; }

  /** @brief Zero configuration: identity base pose, zero joints. */
  [[nodiscard]] Eigen::VectorXd neutral_configuration() const;

  /** @brief Forward kinematics for every body. */
  [[nodiscard]] std::vector<Pose> forward_kinematics(const Eigen::VectorXd& q) const;

  /** @brief World-frame pose of one body (throws on unknown id). */
  [[nodiscard]] Pose body_pose(const Eigen::VectorXd& q, const std::string& body_id) const;

  /**
   * @brief 6x nv Jacobian of a body, rows [linear; angular], at the body origin.
   */
  [[nodiscard]] Eigen::MatrixXd jacobian(const Eigen::VectorXd& q,
                                         const std::string& body_id) const;

  /** @brief Joint-space mass matrix M(q), nv x nv, symmetric positive definite. */
  [[nodiscard]] Eigen::MatrixXd mass_matrix(const Eigen::VectorXd& q) const;

  /** @brief Generalised force of gravity, Q_g (nv). */
  [[nodiscard]] Eigen::VectorXd gravity_force(const Eigen::VectorXd& q) const;

  /**
   * @brief Bias vector h(q, v) = C(q, v) v - Q_g.
   * @note The Coriolis part uses central differences of M; see the file header.
   */
  [[nodiscard]] Eigen::VectorXd bias_forces(const Eigen::VectorXd& q,
                                            const Eigen::VectorXd& v) const;

  /** @brief Kinetic energy 0.5 * v^T M v. */
  [[nodiscard]] double kinetic_energy(const Eigen::VectorXd& q, const Eigen::VectorXd& v) const;

  /** @brief Potential energy relative to the base height (m g z_com). */
  [[nodiscard]] double potential_energy(const Eigen::VectorXd& q) const;

  /** @brief Accrued linear and angular momentum about the world origin. */
  [[nodiscard]] Eigen::Matrix<double, 6, 1> momentum(const Eigen::VectorXd& q,
                                                     const Eigen::VectorXd& v) const;

  /** @brief Rotation of the base body, world <- body. */
  [[nodiscard]] Eigen::Matrix3d base_rotation(const Eigen::VectorXd& q) const;

  /**
   * @brief Converts a world-frame angular velocity to the body frame.
   *
   * The state contract stores `omega_b` (docs/03 section 4) while this solver
   * integrates world-frame angular velocity; the runtime converts at the
   * boundary and never assumes which frame a plugin uses.
   */
  [[nodiscard]] Eigen::Vector3d omega_world_to_body(const Eigen::VectorXd& q,
                                                    const Eigen::Vector3d& omega_world) const;

  /** @brief Converts a body-frame angular velocity to the world frame. */
  [[nodiscard]] Eigen::Vector3d omega_body_to_world(const Eigen::VectorXd& q,
                                                    const Eigen::Vector3d& omega_body) const;

  /** @brief Replaces the uniform gravity vector (default: 9.80665 m/s^2 down). */
  void set_gravity(const Eigen::Vector3d& gravity) { gravity_ = gravity; }
  [[nodiscard]] const Eigen::Vector3d& gravity() const noexcept { return gravity_; }

  /** @brief Number of actuators declared by the model. */
  [[nodiscard]] int actuator_count() const noexcept { return static_cast<int>(actuators_.size()); }

  /**
   * @brief Maps actuator commands to generalised forces.
   * @param commands One value per actuator, in model order (N or N*m).
   *
   * Torque actuators add to the joint coordinate; wheels apply a pure torque
   * about their axis on the host body; thrusters apply a body-fixed force at
   * the body origin. Commands are clamped to `max_effort`.
   */
  [[nodiscard]] Eigen::VectorXd actuator_force(const Eigen::VectorXd& q,
                                               const Eigen::VectorXd& commands) const;

  /**
   * @brief Generalised force of a world-frame force applied at a world point.
   * @param q Configuration (for the Jacobian).
   * @param body_id Body that receives the force.
   * @param force_world Force vector in the world frame, N.
   * @param point_world Application point in the world frame, m.
   */
  [[nodiscard]] Eigen::VectorXd generalized_force(const Eigen::VectorXd& q,
                                                  const std::string& body_id,
                                                  const Eigen::Vector3d& force_world,
                                                  const Eigen::Vector3d& point_world) const;

  /** @brief Callback returning external generalised forces at (q, v). */
  using ForceFunction =
      std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)>;

  /**
   * @brief Integrates one step with classical RK4 and a state-dependent force.
   *
   * Use this overload whenever the external force depends on the state (orbital
   * gravity, drag, third-body terms): evaluating it at every RK4 stage keeps the
   * method fourth order, whereas holding it constant per step degrades it to
   * first order in the forcing.
   */
  void step(Eigen::VectorXd* q, Eigen::VectorXd* v, const ForceFunction& external, double dt) const;

  /**
   * @brief Integrates one step with classical RK4.
   * @param q Configuration, updated in place (quaternion renormalised).
   * @param v Velocity, updated in place.
   * @param tau Generalised force (nv); zero means free motion.
   * @param dt Step size, s.
   */
  void step(Eigen::VectorXd* q, Eigen::VectorXd* v, const Eigen::VectorXd& tau, double dt) const;

 private:
  struct BodyState {
    std::string id;
    int parent = -1;  ///< Parent body index, -1 for the base.
    int joint = -1;   ///< Incoming joint index, -1 for the base.
    double mass = 0.0;
    Eigen::Vector3d com{0.0, 0.0, 0.0};                ///< COM in the body frame.
    Eigen::Matrix3d inertia{Eigen::Matrix3d::Zero()};  ///< About the COM.
  };
  struct JointState {
    std::string id;
    int parent_body = -1;
    int child_body = -1;
    srsp::model::JointType type = srsp::model::JointType::kRevolute;
    Eigen::Vector3d axis{0.0, 0.0, 1.0};
    Eigen::Vector3d origin{0.0, 0.0, 0.0};
    Eigen::Matrix3d origin_rotation{Eigen::Matrix3d::Identity()};
    double lower = 0.0;
    double upper = 0.0;
  };

  struct ActuatorState {
    std::string id;
    srsp::model::ActuatorType type = srsp::model::ActuatorType::kTorque;
    int joint = -1;                       ///< Joint index for torque actuators.
    int body = -1;                        ///< Body index for wheels/thrusters.
    Eigen::Vector3d axis{1.0, 0.0, 0.0};  ///< Mode axis in the body frame.
    double max_effort = 0.0;              ///< Saturation.
  };

  Eigen::Matrix3d gravity_rotation() const;

  /** @brief 6 x nv Jacobians of every body, computed in one pass. */
  std::vector<Eigen::MatrixXd> compute_jacobians(const Eigen::VectorXd& q) const;

  int nq_ = 0;
  int nv_ = 0;
  int joint_count_ = 0;
  int base_ = 0;
  std::vector<BodyState> bodies_;
  std::vector<JointState> joints_;
  std::vector<ActuatorState> actuators_;
  std::vector<std::string> body_ids_;
  std::unordered_map<std::string, int> body_index_;
  std::unordered_map<std::string, int> joint_index_;
  Eigen::Vector3d gravity_{0.0, 0.0, -9.80665};
  double finite_difference_step_ = 1e-6;
};

}  // namespace srsp::dynamics

#endif  // SRSP_DYNAMICS_RIGID_BODY_HPP
