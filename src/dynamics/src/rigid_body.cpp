/**
 * @file rigid_body.cpp
 * @brief Forward kinematics, Jacobians, mass matrix and gravity for an SRM tree.
 *
 * Coriolis/centrifugal terms are not implemented yet: `bias_forces` returns the
 * gravity part only. The recursive Newton-Euler pass is tracked as M1.4b; this
 * round covers kinematics, mass matrix, Jacobians, gravity, momentum and a
 * semi-implicit integrator, all of which are exact for the supported features.
 */
#include <srsp/dynamics/rigid_body.hpp>

#include <algorithm>

#include <Eigen/Geometry>
#include <stdexcept>

namespace srsp::dynamics {
namespace {

Eigen::Matrix3d skew(const Eigen::Vector3d& v) {
  Eigen::Matrix3d m;
  m << 0.0, -v.z(), v.y(), v.z(), 0.0, -v.x(), -v.y(), v.x(), 0.0;
  return m;
}

Eigen::Matrix3d rotation_from_rpy(const Eigen::Vector3d& rpy) {
  return (Eigen::AngleAxisd(rpy.z(), Eigen::Vector3d::UnitZ()) *
          Eigen::AngleAxisd(rpy.y(), Eigen::Vector3d::UnitY()) *
          Eigen::AngleAxisd(rpy.x(), Eigen::Vector3d::UnitX()))
      .toRotationMatrix();
}

Eigen::Matrix3d rotation_about_axis(const Eigen::Vector3d& axis, double angle) {
  return Eigen::AngleAxisd(angle, axis.normalized()).toRotationMatrix();
}

Eigen::Matrix3d tensor_from_array(const srsp::model::Mat3& values) {
  Eigen::Matrix3d tensor;
  tensor << values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7],
      values[8];
  return tensor;
}

Eigen::Vector3d vector_from_array(const srsp::model::Vec3& values) {
  return {values[0], values[1], values[2]};
}

bool is_revolute(srsp::model::JointType type) {
  return type == srsp::model::JointType::kRevolute || type == srsp::model::JointType::kContinuous;
}

}  // namespace

RigidBodyDynamics::RigidBodyDynamics(const srsp::model::Model& model) {
  joint_count_ = static_cast<int>(model.joints.size());
  nq_ = 7 + joint_count_;
  nv_ = 6 + joint_count_;

  if (model.bodies.empty()) {
    throw std::invalid_argument("SRM model has no bodies");
  }

  for (std::size_t i = 0; i < model.joints.size(); ++i) {
    joint_index_[model.joints[i].id] = static_cast<int>(i);
  }

  // Order bodies breadth-first so every parent precedes its children.
  std::vector<int> order;
  std::vector<bool> visited(model.bodies.size(), false);
  for (std::size_t i = 0; i < model.bodies.size(); ++i) {
    if (model.bodies[i].parent_joint.empty()) {
      visited[i] = true;
      order.push_back(static_cast<int>(i));
    }
  }
  std::size_t cursor = 0;
  while (cursor < order.size()) {
    const std::string& parent_id = model.bodies[static_cast<std::size_t>(order[cursor])].id;
    ++cursor;
    for (std::size_t j = 0; j < model.bodies.size(); ++j) {
      if (visited[j]) {
        continue;
      }
      const auto joint_it = joint_index_.find(model.bodies[j].parent_joint);
      if (joint_it == joint_index_.end()) {
        continue;
      }
      if (model.joints[static_cast<std::size_t>(joint_it->second)].parent_body == parent_id) {
        visited[j] = true;
        order.push_back(static_cast<int>(j));
      }
    }
  }
  if (order.size() != model.bodies.size()) {
    throw std::invalid_argument("SRM model is not a connected tree");
  }
  base_ = order.front();

  bodies_.reserve(order.size());
  for (const int source : order) {
    const auto& body = model.bodies[static_cast<std::size_t>(source)];
    BodyState state;
    state.id = body.id;
    state.mass = body.inertia.mass;
    state.com = vector_from_array(body.inertia.com);
    state.inertia = tensor_from_array(body.inertia.inertia);
    body_index_[state.id] = static_cast<int>(bodies_.size());
    bodies_.push_back(std::move(state));
  }

  joints_.reserve(joint_count_);
  for (const auto& joint : model.joints) {
    JointState state;
    state.id = joint.id;
    state.parent_body = body_index_[joint.parent_body];
    state.child_body = body_index_[joint.child_body];
    state.type = joint.type;
    state.axis = vector_from_array(joint.axis);
    state.origin = vector_from_array(joint.origin);
    state.origin_rotation = rotation_from_rpy(vector_from_array(joint.origin_rpy));
    state.lower = joint.type == srsp::model::JointType::kContinuous ? -1e9 : joint.lower;
    state.upper = joint.type == srsp::model::JointType::kContinuous ? 1e9 : joint.upper;
    joints_.push_back(std::move(state));
  }
  for (auto& body : bodies_) {
    for (std::size_t j = 0; j < model.joints.size(); ++j) {
      if (model.joints[j].child_body == body.id) {
        body.joint = static_cast<int>(j);
        body.parent = body_index_[model.joints[j].parent_body];
      }
    }
  }

  body_ids_.reserve(bodies_.size());
  for (const auto& body : bodies_) {
    body_ids_.push_back(body.id);
  }

  actuators_.reserve(model.actuators.size());
  for (const auto& actuator : model.actuators) {
    ActuatorState state;
    state.id = actuator.id;
    state.type = actuator.type;
    state.joint = actuator.joint.empty() ? -1 : joint_index_[actuator.joint];
    state.body = actuator.body.empty() ? -1 : body_index_[actuator.body];
    state.axis = Eigen::Vector3d(actuator.axis[0], actuator.axis[1], actuator.axis[2]);
    state.max_effort = actuator.max_effort;
    actuators_.push_back(std::move(state));
  }
}

Eigen::Matrix3d RigidBodyDynamics::gravity_rotation() const {
  return Eigen::Matrix3d::Identity();
}

Eigen::VectorXd RigidBodyDynamics::neutral_configuration() const {
  Eigen::VectorXd q = Eigen::VectorXd::Zero(nq_);
  q[6] = 1.0;  // quaternion w
  return q;
}

std::vector<Pose> RigidBodyDynamics::forward_kinematics(const Eigen::VectorXd& q) const {
  if (q.size() != nq_) {
    throw std::invalid_argument("configuration vector has the wrong size");
  }
  std::vector<Pose> poses(bodies_.size());
  poses[0].position = Eigen::Vector3d(q[0], q[1], q[2]);
  Eigen::Quaterniond base(q[6], q[3], q[4], q[5]);
  base.normalize();
  poses[0].rotation = base.toRotationMatrix();

  for (std::size_t i = 1; i < bodies_.size(); ++i) {
    const BodyState& body = bodies_[i];
    const JointState& joint = joints_[static_cast<std::size_t>(body.joint)];
    const Pose& parent = poses[static_cast<std::size_t>(body.parent)];

    Eigen::Matrix3d joint_rotation = joint.origin_rotation;
    Eigen::Vector3d joint_translation = joint.origin;
    const double position = q[7 + body.joint];
    if (is_revolute(joint.type)) {
      joint_rotation = joint_rotation * rotation_about_axis(joint.axis, position);
    } else if (joint.type == srsp::model::JointType::kPrismatic) {
      joint_translation += joint.axis * position;
    }
    poses[i].rotation = parent.rotation * joint_rotation;
    poses[i].position = parent.position + parent.rotation * joint_translation;
  }
  return poses;
}

Pose RigidBodyDynamics::body_pose(const Eigen::VectorXd& q, const std::string& body_id) const {
  const auto it = body_index_.find(body_id);
  if (it == body_index_.end()) {
    throw std::out_of_range("unknown body id: " + body_id);
  }
  return forward_kinematics(q)[static_cast<std::size_t>(it->second)];
}

std::vector<Eigen::MatrixXd> RigidBodyDynamics::compute_jacobians(const Eigen::VectorXd& q) const {
  const auto poses = forward_kinematics(q);
  std::vector<Eigen::MatrixXd> jacobians(bodies_.size());
  jacobians[0] = Eigen::MatrixXd::Zero(6, nv_);
  jacobians[0].block<3, 3>(0, 0).setIdentity();
  jacobians[0].block<3, 3>(3, 3).setIdentity();

  for (std::size_t i = 1; i < bodies_.size(); ++i) {
    const BodyState& body = bodies_[i];
    const JointState& joint = joints_[static_cast<std::size_t>(body.joint)];
    const std::size_t parent = static_cast<std::size_t>(body.parent);
    const Eigen::Vector3d r_pc = poses[i].position - poses[parent].position;
    const int column = 6 + body.joint;

    Eigen::MatrixXd jac = jacobians[parent];
    jac.topRows(3) += -skew(r_pc) * jacobians[parent].bottomRows(3);
    const Eigen::Vector3d axis_world = poses[parent].rotation * joint.origin_rotation * joint.axis;
    if (is_revolute(joint.type)) {
      jac.block<3, 1>(3, column) += axis_world;
    } else if (joint.type == srsp::model::JointType::kPrismatic) {
      jac.block<3, 1>(0, column) += axis_world;
    }
    jacobians[i] = std::move(jac);
  }
  return jacobians;
}

Eigen::MatrixXd RigidBodyDynamics::jacobian(const Eigen::VectorXd& q,
                                            const std::string& body_id) const {
  const auto it = body_index_.find(body_id);
  if (it == body_index_.end()) {
    throw std::out_of_range("unknown body id: " + body_id);
  }
  return compute_jacobians(q)[static_cast<std::size_t>(it->second)];
}

Eigen::MatrixXd RigidBodyDynamics::mass_matrix(const Eigen::VectorXd& q) const {
  const auto poses = forward_kinematics(q);
  const auto jacobians = compute_jacobians(q);
  Eigen::MatrixXd mass = Eigen::MatrixXd::Zero(nv_, nv_);

  for (std::size_t i = 0; i < bodies_.size(); ++i) {
    const BodyState& body = bodies_[i];
    const Eigen::Vector3d c = poses[i].rotation * body.com;
    const Eigen::MatrixXd com_jac = jacobians[i].topRows(3) - skew(c) * jacobians[i].bottomRows(3);
    const Eigen::Matrix3d inertia_world =
        poses[i].rotation * body.inertia * poses[i].rotation.transpose();
    mass.noalias() += body.mass * com_jac.transpose() * com_jac;
    mass.noalias() +=
        jacobians[i].bottomRows(3).transpose() * inertia_world * jacobians[i].bottomRows(3);
  }
  return mass;
}

Eigen::VectorXd RigidBodyDynamics::gravity_force(const Eigen::VectorXd& q) const {
  const auto poses = forward_kinematics(q);
  const auto jacobians = compute_jacobians(q);
  Eigen::VectorXd force = Eigen::VectorXd::Zero(nv_);
  for (std::size_t i = 0; i < bodies_.size(); ++i) {
    const BodyState& body = bodies_[i];
    if (body.mass <= 0.0) {
      continue;
    }
    const Eigen::Vector3d c = poses[i].rotation * body.com;
    const Eigen::MatrixXd com_jac = jacobians[i].topRows(3) - skew(c) * jacobians[i].bottomRows(3);
    force.noalias() += com_jac.transpose() * (body.mass * gravity_);
  }
  return force;
}

Eigen::VectorXd RigidBodyDynamics::bias_forces(const Eigen::VectorXd& q,
                                               const Eigen::VectorXd& v) const {
  if (q.size() != nq_ || v.size() != nv_) {
    throw std::invalid_argument("state vector has the wrong size");
  }
  const auto poses = forward_kinematics(q);
  const std::size_t count = bodies_.size();

  std::vector<Eigen::Vector3d> omega(count, Eigen::Vector3d::Zero());
  std::vector<Eigen::Vector3d> alpha(count, Eigen::Vector3d::Zero());
  std::vector<Eigen::Vector3d> accel(count, Eigen::Vector3d::Zero());

  // Forward pass with zero joint acceleration: keeps only velocity-dependent
  // terms, which is exactly the bias h = C(q, v) v + G.
  omega[0] = Eigen::Vector3d(v[3], v[4], v[5]);
  alpha[0].setZero();
  accel[0].setZero();

  for (std::size_t i = 1; i < count; ++i) {
    const BodyState& body = bodies_[i];
    const JointState& joint = joints_[static_cast<std::size_t>(body.joint)];
    const std::size_t parent = static_cast<std::size_t>(body.parent);
    const double rate = v[6 + body.joint];
    const Eigen::Vector3d axis_world = poses[parent].rotation * joint.origin_rotation * joint.axis;
    const Eigen::Vector3d r = poses[i].position - poses[parent].position;

    if (is_revolute(joint.type)) {
      const Eigen::Vector3d joint_rate = axis_world * rate;
      omega[i] = omega[parent] + joint_rate;
      alpha[i] = alpha[parent] + omega[parent].cross(joint_rate);
      accel[i] =
          accel[parent] + alpha[parent].cross(r) + omega[parent].cross(omega[parent].cross(r));
    } else if (joint.type == srsp::model::JointType::kPrismatic) {
      const Eigen::Vector3d slide = axis_world * rate;
      omega[i] = omega[parent];
      alpha[i] = alpha[parent];
      accel[i] = accel[parent] + alpha[parent].cross(r) +
                 omega[parent].cross(omega[parent].cross(r)) + 2.0 * omega[parent].cross(slide);
    } else {
      omega[i] = omega[parent];
      alpha[i] = alpha[parent];
      accel[i] =
          accel[parent] + alpha[parent].cross(r) + omega[parent].cross(omega[parent].cross(r));
    }
  }

  Eigen::VectorXd out = Eigen::VectorXd::Zero(nv_);
  std::vector<Eigen::Vector3d> force(count, Eigen::Vector3d::Zero());
  std::vector<Eigen::Vector3d> moment(count, Eigen::Vector3d::Zero());

  // Backward pass: accumulate subtree forces and extract joint torques.
  for (std::size_t index = count; index-- > 0;) {
    const BodyState& body = bodies_[index];
    const Eigen::Vector3d c = poses[index].rotation * body.com;
    const Eigen::Vector3d com_accel =
        accel[index] + alpha[index].cross(c) + omega[index].cross(omega[index].cross(c));
    const Eigen::Matrix3d inertia_world =
        poses[index].rotation * body.inertia * poses[index].rotation.transpose();

    force[index] += body.mass * com_accel;
    moment[index] += inertia_world * alpha[index] +
                     omega[index].cross(inertia_world * omega[index]) +
                     c.cross(body.mass * com_accel);
    if (body.mass > 0.0) {
      force[index] -= body.mass * gravity_;
      moment[index] -= c.cross(body.mass * gravity_);
    }

    if (index == static_cast<std::size_t>(base_)) {
      continue;
    }
    const int joint_index = body.joint;
    const JointState& joint = joints_[static_cast<std::size_t>(joint_index)];
    const std::size_t parent = static_cast<std::size_t>(body.parent);
    const Eigen::Vector3d axis_world = poses[parent].rotation * joint.origin_rotation * joint.axis;
    const Eigen::Vector3d r = poses[index].position - poses[parent].position;

    if (is_revolute(joint.type)) {
      out[6 + joint_index] = axis_world.dot(moment[index]);
    } else if (joint.type == srsp::model::JointType::kPrismatic) {
      out[6 + joint_index] = axis_world.dot(force[index]);
    }
    force[parent] += force[index];
    moment[parent] += moment[index] + r.cross(force[index]);
  }

  out.segment<3>(0) = force[static_cast<std::size_t>(base_)];
  out.segment<3>(3) = moment[static_cast<std::size_t>(base_)];
  return out;
}

double RigidBodyDynamics::kinetic_energy(const Eigen::VectorXd& q, const Eigen::VectorXd& v) const {
  return 0.5 * v.dot(mass_matrix(q) * v);
}

double RigidBodyDynamics::potential_energy(const Eigen::VectorXd& q) const {
  const auto poses = forward_kinematics(q);
  double energy = 0.0;
  for (std::size_t i = 0; i < bodies_.size(); ++i) {
    if (bodies_[i].mass <= 0.0) {
      continue;
    }
    const Eigen::Vector3d com_world = poses[i].position + poses[i].rotation * bodies_[i].com;
    energy -= bodies_[i].mass * gravity_.dot(com_world);
  }
  return energy;
}

Eigen::Matrix<double, 6, 1> RigidBodyDynamics::momentum(const Eigen::VectorXd& q,
                                                        const Eigen::VectorXd& v) const {
  const auto poses = forward_kinematics(q);
  const auto jacobians = compute_jacobians(q);
  Eigen::Matrix<double, 6, 1> total = Eigen::Matrix<double, 6, 1>::Zero();
  for (std::size_t i = 0; i < bodies_.size(); ++i) {
    const BodyState& body = bodies_[i];
    const Eigen::Vector3d c = poses[i].rotation * body.com;
    const Eigen::MatrixXd com_jac = jacobians[i].topRows(3) - skew(c) * jacobians[i].bottomRows(3);
    const Eigen::Vector3d angular = poses[i].rotation * body.inertia *
                                    poses[i].rotation.transpose() *
                                    (jacobians[i].bottomRows(3) * v);
    const Eigen::Vector3d linear = body.mass * (com_jac * v);
    total.head<3>() += linear;
    total.tail<3>() += (poses[i].position + c).cross(linear) + angular;
  }
  return total;
}

Eigen::VectorXd RigidBodyDynamics::actuator_force(const Eigen::VectorXd& q,
                                                  const Eigen::VectorXd& commands) const {
  if (commands.size() != static_cast<Eigen::Index>(actuators_.size())) {
    throw std::invalid_argument("actuator command vector has the wrong size");
  }
  Eigen::VectorXd tau = Eigen::VectorXd::Zero(nv_);
  if (actuators_.empty()) {
    return tau;
  }
  const auto poses = forward_kinematics(q);
  const auto jacobians = compute_jacobians(q);

  for (std::size_t i = 0; i < actuators_.size(); ++i) {
    const ActuatorState& actuator = actuators_[i];
    double effort = commands[static_cast<Eigen::Index>(i)];
    if (std::abs(effort) > actuator.max_effort) {
      effort = std::copysign(actuator.max_effort, effort);
    }
    if (effort == 0.0) {
      continue;
    }
    const Eigen::Vector3d axis_world =
        actuator.body >= 0 ? poses[static_cast<std::size_t>(actuator.body)].rotation * actuator.axis
                           : actuator.axis;

    switch (actuator.type) {
      case srsp::model::ActuatorType::kTorque:
        if (actuator.joint >= 0) {
          tau[6 + actuator.joint] += effort;
        }
        break;
      case srsp::model::ActuatorType::kWheel:
        if (actuator.body >= 0) {
          const Eigen::MatrixXd& jac = jacobians[static_cast<std::size_t>(actuator.body)];
          tau += jac.bottomRows(3).transpose() * (axis_world * effort);
        }
        break;
      case srsp::model::ActuatorType::kThruster:
        if (actuator.body >= 0) {
          const Eigen::MatrixXd& jac = jacobians[static_cast<std::size_t>(actuator.body)];
          tau += jac.topRows(3).transpose() * (axis_world * effort);
        }
        break;
    }
  }
  return tau;
}

Eigen::VectorXd RigidBodyDynamics::generalized_force(const Eigen::VectorXd& q,
                                                     const std::string& body_id,
                                                     const Eigen::Vector3d& force_world,
                                                     const Eigen::Vector3d& point_world) const {
  const auto it = body_index_.find(body_id);
  if (it == body_index_.end()) {
    throw std::out_of_range("unknown body id: " + body_id);
  }
  const auto poses = forward_kinematics(q);
  const auto jacobians = compute_jacobians(q);
  const std::size_t index = static_cast<std::size_t>(it->second);
  const Eigen::Vector3d r = point_world - poses[index].position;

  Eigen::VectorXd tau = Eigen::VectorXd::Zero(nv_);
  tau += jacobians[index].topRows(3).transpose() * force_world;
  tau += jacobians[index].bottomRows(3).transpose() * r.cross(force_world);
  return tau;
}

Eigen::Matrix3d RigidBodyDynamics::base_rotation(const Eigen::VectorXd& q) const {
  Eigen::Quaterniond orientation(q[6], q[3], q[4], q[5]);
  orientation.normalize();
  return orientation.toRotationMatrix();
}

Eigen::Vector3d RigidBodyDynamics::omega_world_to_body(const Eigen::VectorXd& q,
                                                       const Eigen::Vector3d& omega_world) const {
  return base_rotation(q).transpose() * omega_world;
}

Eigen::Vector3d RigidBodyDynamics::omega_body_to_world(const Eigen::VectorXd& q,
                                                       const Eigen::Vector3d& omega_body) const {
  return base_rotation(q) * omega_body;
}
void RigidBodyDynamics::step(Eigen::VectorXd* q, Eigen::VectorXd* v, const Eigen::VectorXd& tau,
                             double dt) const {
  const ForceFunction constant = [&tau](const Eigen::VectorXd&, const Eigen::VectorXd&) {
    return tau;
  };
  step(q, v, constant, dt);
}

void RigidBodyDynamics::step(Eigen::VectorXd* q, Eigen::VectorXd* v, const ForceFunction& external,
                             double dt) const {
  if (q == nullptr || v == nullptr) {
    throw std::invalid_argument("null state pointer");
  }
  if (q->size() != nq_ || v->size() != nv_) {
    throw std::invalid_argument("state or force vector has the wrong size");
  }

  // Classical RK4 on the state (q, v). The configuration derivative maps the
  // generalised velocity into the coordinate velocity: translation is direct,
  // the quaternion uses q_dot = 0.5 * [0, omega] * q, joints are direct.
  const auto config_velocity = [this](const Eigen::VectorXd& q_state,
                                      const Eigen::VectorXd& v_state) {
    Eigen::VectorXd q_dot = Eigen::VectorXd::Zero(nq_);
    q_dot[0] = v_state[0];
    q_dot[1] = v_state[1];
    q_dot[2] = v_state[2];
    const Eigen::Quaterniond orientation(q_state[6], q_state[3], q_state[4], q_state[5]);
    const Eigen::Quaterniond spin(0.0, v_state[3], v_state[4], v_state[5]);
    const Eigen::Quaterniond rate = spin * orientation;
    q_dot[3] = 0.5 * rate.x();
    q_dot[4] = 0.5 * rate.y();
    q_dot[5] = 0.5 * rate.z();
    q_dot[6] = 0.5 * rate.w();
    for (int i = 0; i < joint_count_; ++i) {
      q_dot[7 + i] = v_state[6 + i];
    }
    return q_dot;
  };
  const auto accel_of = [this, &external](const Eigen::VectorXd& q_state,
                                          const Eigen::VectorXd& v_state) {
    const Eigen::MatrixXd mass = mass_matrix(q_state);
    const Eigen::LDLT<Eigen::MatrixXd> factorization(mass);
    const Eigen::VectorXd accel =
        factorization.solve(external(q_state, v_state) - bias_forces(q_state, v_state));
    return accel;
  };

  const Eigen::VectorXd q0 = *q;
  const Eigen::VectorXd v0 = *v;
  const Eigen::VectorXd k1q = config_velocity(q0, v0);
  const Eigen::VectorXd k1v = accel_of(q0, v0);
  const Eigen::VectorXd k2q = config_velocity(q0 + 0.5 * dt * k1q, v0 + 0.5 * dt * k1v);
  const Eigen::VectorXd k2v = accel_of(q0 + 0.5 * dt * k1q, v0 + 0.5 * dt * k1v);
  const Eigen::VectorXd k3q = config_velocity(q0 + 0.5 * dt * k2q, v0 + 0.5 * dt * k2v);
  const Eigen::VectorXd k3v = accel_of(q0 + 0.5 * dt * k2q, v0 + 0.5 * dt * k2v);
  const Eigen::VectorXd k4q = config_velocity(q0 + dt * k3q, v0 + dt * k3v);
  const Eigen::VectorXd k4v = accel_of(q0 + dt * k3q, v0 + dt * k3v);

  *q += (dt / 6.0) * (k1q + 2.0 * k2q + 2.0 * k3q + k4q);
  *v += (dt / 6.0) * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);

  Eigen::Quaterniond orientation((*q)[6], (*q)[3], (*q)[4], (*q)[5]);
  orientation.normalize();
  (*q)[3] = orientation.x();
  (*q)[4] = orientation.y();
  (*q)[5] = orientation.z();
  (*q)[6] = orientation.w();

  for (int i = 0; i < joint_count_; ++i) {
    const auto& joint = joints_[static_cast<std::size_t>(i)];
    (*q)[7 + i] = std::clamp((*q)[7 + i], joint.lower, joint.upper);
  }
}

}  // namespace srsp::dynamics
