/**
 * @file rigid_body.hpp
 * @brief Floating-base rigid-body dynamics over an SRM model.
 * @brief_zh 基于 SRM
 * 模型的浮动基刚体动力学。
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
/** @brief_zh 体在世界系中的位姿。 */
struct Pose {
  Eigen::Vector3d position{0.0, 0.0, 0.0};                ///< Body origin, m. / 体原点位置（米）。
  Eigen::Matrix3d rotation{Eigen::Matrix3d::Identity()};  ///< Body -> world. / 体到世界的旋转。
};

/** @brief Floating-base rigid-body model built from an SRM description. */
/** @brief_zh 由 SRM 描述构建的浮动基刚体模型。 */
class RigidBodyDynamics {
 public:
  /**
   * @brief Compiles the kinematic tree and inertia properties.
   * @brief_zh
   * 编译运动学树与惯量属性。
   * @param model Validated SRM model.
   */
  explicit RigidBodyDynamics(const srsp::model::Model& model);

  [[nodiscard]] int nq() const noexcept { return nq_; }
  [[nodiscard]] int nv() const noexcept { return nv_; }
  [[nodiscard]] int joint_count() const noexcept { return joint_count_; }
  [[nodiscard]] const std::vector<std::string>& body_ids() const noexcept { return body_ids_; }

  /** @brief Zero configuration: identity base pose, zero joints. */
  /** @brief_zh 零位构型：基座位姿为单位、关节角为零。 */
  [[nodiscard]] Eigen::VectorXd neutral_configuration() const;

  /** @brief Forward kinematics for every body. */
  /** @brief_zh 计算所有体的正运动学。 */
  [[nodiscard]] std::vector<Pose> forward_kinematics(const Eigen::VectorXd& q) const;

  /** @brief World-frame pose of one body (throws on unknown id). */
  /** @brief_zh 单个体的世界系位姿（未知 id 时抛异常）。 */
  [[nodiscard]] Pose body_pose(const Eigen::VectorXd& q, const std::string& body_id) const;

  /**
   * @brief 6x nv Jacobian of a body, rows [linear; angular], at the body origin.
   * @brief_zh
   * 单体的 6×nv 雅可比，行序 [线速度; 角速度]，取在体原点。
   */
  [[nodiscard]] Eigen::MatrixXd jacobian(const Eigen::VectorXd& q,
                                         const std::string& body_id) const;

  /** @brief Joint-space mass matrix M(q), nv x nv, symmetric positive definite. */
  /** @brief_zh 关节空间质量矩阵 M(q)，nv×nv，对称正定。 */
  [[nodiscard]] Eigen::MatrixXd mass_matrix(const Eigen::VectorXd& q) const;

  /**
   * @brief Reference mass matrix: sums every body contribution over all dofs.
   * @brief_zh
   * 参考质量矩阵：对所有自由度累加每个体的贡献（慢速对照实现）。
   *
   * Slow but direct; used to validate the optimised assembly.
   */
  [[nodiscard]] Eigen::MatrixXd mass_matrix_reference(const Eigen::VectorXd& q) const;

  /**
   * @brief Composite-rigid-body mass matrix (experimental, O(n) assembly).
   * @brief_zh
   * 复合刚体法质量矩阵（实验，O(n) 装配）。
   */
  [[nodiscard]] Eigen::MatrixXd mass_matrix_crba(const Eigen::VectorXd& q) const;

  /**
   * @brief Experimental O(n) accelerations (ABA); not used by step() yet.
   * @brief_zh
   * 实验性 O(n) 加速度（ABA），尚未被 step() 使用。
   */
  [[nodiscard]] Eigen::Matrix<double, 6, 6> spatial_inertia(std::size_t body,
                                                            const Pose& pose) const;

  [[nodiscard]] Eigen::VectorXd aba_accelerations(const Eigen::VectorXd& q,
                                                  const Eigen::VectorXd& v,
                                                  const Eigen::VectorXd& tau) const;

  /** @brief Generalised force of gravity, Q_g (nv). */
  /** @brief_zh 重力广义力 Q_g（nv 维）。 */
  [[nodiscard]] Eigen::VectorXd gravity_force(const Eigen::VectorXd& q) const;

  /**
   * @brief Bias vector h(q, v) = C(q, v) v - Q_g.
   * @brief_zh 偏置向量 h(q,v) = C(q,v)v −
   * Q_g（含科氏力与重力）。
   * @note The Coriolis part uses central differences of M; see the file header.
   */
  [[nodiscard]] Eigen::VectorXd bias_forces(const Eigen::VectorXd& q,
                                            const Eigen::VectorXd& v) const;

  /** @brief Kinetic energy 0.5 * v^T M v. */
  /** @brief_zh 动能 0.5·vᵀMv。 */
  [[nodiscard]] double kinetic_energy(const Eigen::VectorXd& q, const Eigen::VectorXd& v) const;

  /** @brief Potential energy relative to the base height (m g z_com). */
  /** @brief_zh 相对基座高度的势能（m·g·z_com）。 */
  [[nodiscard]] double potential_energy(const Eigen::VectorXd& q) const;

  /** @brief Accrued linear and angular momentum about the world origin. */
  /** @brief_zh 相对世界原点的线动量与角动量。 */
  [[nodiscard]] Eigen::Matrix<double, 6, 1> momentum(const Eigen::VectorXd& q,
                                                     const Eigen::VectorXd& v) const;

  /** @brief Rotation of the base body, world <- body. */
  /** @brief_zh 基座旋转，世界←体。 */
  [[nodiscard]] Eigen::Matrix3d base_rotation(const Eigen::VectorXd& q) const;

  /**
   * @brief Converts a world-frame angular velocity to the body frame.
   * @brief_zh
   * 将世界系角速度转换到体系。
   *
   * The state contract stores `omega_b` (docs/03 section 4) while this solver
   * integrates world-frame angular velocity; the runtime converts at the
   * boundary and never assumes which frame a plugin uses.
   */
  [[nodiscard]] Eigen::Vector3d omega_world_to_body(const Eigen::VectorXd& q,
                                                    const Eigen::Vector3d& omega_world) const;

  /** @brief Converts a body-frame angular velocity to the world frame. */
  /** @brief_zh 将体系角速度转换到世界系。 */
  [[nodiscard]] Eigen::Vector3d omega_body_to_world(const Eigen::VectorXd& q,
                                                    const Eigen::Vector3d& omega_body) const;

  /** @brief Replaces the uniform gravity vector (default: 9.80665 m/s^2 down). */
  /** @brief_zh 替换均匀重力向量（默认向下 9.80665 m/s²）。 */
  void set_gravity(const Eigen::Vector3d& gravity) { gravity_ = gravity; }
  [[nodiscard]] const Eigen::Vector3d& gravity() const noexcept { return gravity_; }

  /** @brief Number of actuators declared by the model. */
  /** @brief_zh 模型声明的执行器数量。 */
  [[nodiscard]] int actuator_count() const noexcept { return static_cast<int>(actuators_.size()); }

  /**
   * @brief Maps actuator commands to generalised forces.
   * @brief_zh
   * 将执行器指令映射为广义力。
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
   * @brief_zh
   * 世界点处世界系力对应的广义力。
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
  /** @brief_zh 在 (q,v) 处返回外部广义力的回调。 */
  using ForceFunction =
      std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)>;

  /**
   * @brief Integrates one step with classical RK4 and a state-dependent force.
   * @brief_zh
   * 使用经典 RK4 与状态相关外力积分一步。
   *
   * Use this overload whenever the external force depends on the state (orbital
   * gravity, drag, third-body terms): evaluating it at every RK4 stage keeps the
   * method fourth order, whereas holding it constant per step degrades it to
   * first order in the forcing.
   */
  void step(Eigen::VectorXd* q, Eigen::VectorXd* v, const ForceFunction& external, double dt) const;

  /**
   * @brief Integrates one step with classical RK4.
   * @brief_zh 使用经典 RK4 积分一步。
   * @param q Configuration, updated in place (quaternion renormalised).
   * @param v Velocity, updated in place.
   * @param tau Generalised force (nv); zero means free motion.
   * @param dt Step size, s.
   */
  void step(Eigen::VectorXd* q, Eigen::VectorXd* v, const Eigen::VectorXd& tau, double dt) const;

 private:
  struct BodyState {
    std::string id;
    int parent = -1;  ///< Parent body index, -1 for the base. / 父体索引，基座为 −1。
    int joint = -1;   ///< Incoming joint index, -1 for the base. / 入关节索引，基座为 −1。
    double mass = 0.0;
    Eigen::Vector3d com{0.0, 0.0, 0.0};                ///< COM in the body frame. / 体质心位置。
    Eigen::Matrix3d inertia{Eigen::Matrix3d::Zero()};  ///< About the COM. / 绕质心的惯量。
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
    int joint = -1;  ///< Joint index for torque actuators. / 力矩执行器对应关节。
    int body = -1;   ///< Body index for wheels/thrusters. / 飞轮/推力器对应体。
    Eigen::Vector3d axis{1.0, 0.0, 0.0};  ///< Mode axis in the body frame. / 体系中的作用轴。
    double max_effort = 0.0;              ///< Saturation. / 饱和上限。
  };

  Eigen::Matrix3d gravity_rotation() const;

  /** @brief 6 x nv Jacobians of every body, computed in one pass. */
  /** @brief_zh 一次遍历计算所有体的 6×nv 雅可比。 */
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
