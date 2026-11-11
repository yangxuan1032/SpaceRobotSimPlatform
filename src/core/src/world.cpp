/**
 * @file world.cpp
 * @brief Implementation of the multi-entity world (W3).
 */
#include <srsp/core/world.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace srsp::core {

namespace {

std::array<double, 9> rotation_from_quaternion(const std::array<double, 4>& q) {
  const double x = q[0];
  const double y = q[1];
  const double z = q[2];
  const double w = q[3];
  return {1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - z * w),       2.0 * (x * z + y * w),
          2.0 * (x * y + z * w),       1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - x * w),
          2.0 * (x * z - y * w),       2.0 * (y * z + x * w),       1.0 - 2.0 * (x * x + y * y)};
}

std::array<double, 3> mat_vec(const std::array<double, 9>& m, const std::array<double, 3>& v) {
  return {m[0] * v[0] + m[1] * v[1] + m[2] * v[2], m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
          m[6] * v[0] + m[7] * v[1] + m[8] * v[2]};
}

std::array<double, 3> cross3(const std::array<double, 3>& a, const std::array<double, 3>& b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

/** Solves I * x = b for a symmetric positive-definite 3x3 inertia tensor. */
std::array<double, 3> solve3(const std::array<double, 9>& m, const std::array<double, 3>& b) {
  const double det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) +
                     m[2] * (m[3] * m[7] - m[4] * m[6]);
  if (std::abs(det) < 1e-18) {
    return {0.0, 0.0, 0.0};
  }
  const double inv = 1.0 / det;
  const std::array<double, 9> inverse{
      (m[4] * m[8] - m[5] * m[7]) * inv, (m[2] * m[7] - m[1] * m[8]) * inv,
      (m[1] * m[5] - m[2] * m[4]) * inv, (m[5] * m[6] - m[3] * m[8]) * inv,
      (m[0] * m[8] - m[2] * m[6]) * inv, (m[2] * m[3] - m[0] * m[5]) * inv,
      (m[3] * m[7] - m[4] * m[6]) * inv, (m[1] * m[6] - m[0] * m[7]) * inv,
      (m[0] * m[4] - m[1] * m[3]) * inv};
  return mat_vec(inverse, b);
}

std::array<double, 4> quat_mul(const std::array<double, 4>& a, const std::array<double, 4>& b) {
  return {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
          a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
          a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
          a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
}
void update_mass_properties(WorldEntity* entity) {
  if (entity == nullptr || entity->dry_mass_kg <= 0.0) {
    return;
  }
  const double dry = entity->dry_mass_kg;
  const double prop = entity->propellant_kg;
  const double total = dry + prop;
  if (total <= 0.0) {
    return;
  }
  entity->mass_kg = total;
  for (int axis = 0; axis < 3; ++axis) {
    const auto index = static_cast<std::size_t>(axis);
    entity->center_of_mass_m[index] =
        (dry * entity->dry_com_m[index] + prop * entity->tank_com_m[index]) / total;
  }
  // Parallel-axis term for both point masses about the combined centre.
  std::array<double, 9> inertia = entity->dry_inertia;
  const auto add_point_mass = [&inertia](double mass, const std::array<double, 3>& source,
                                         const std::array<double, 3>& center) {
    const std::array<double, 3> d{source[0] - center[0], source[1] - center[1],
                                  source[2] - center[2]};
    const double d2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    const double diagonal[3] = {d2 - d[0] * d[0], d2 - d[1] * d[1], d2 - d[2] * d[2]};
    inertia[0] += mass * diagonal[0];
    inertia[4] += mass * diagonal[1];
    inertia[8] += mass * diagonal[2];
    inertia[1] -= mass * d[0] * d[1];
    inertia[3] -= mass * d[0] * d[1];
    inertia[2] -= mass * d[0] * d[2];
    inertia[6] -= mass * d[0] * d[2];
    inertia[5] -= mass * d[1] * d[2];
    inertia[7] -= mass * d[1] * d[2];
  };
  add_point_mass(dry, entity->dry_com_m, entity->center_of_mass_m);
  add_point_mass(prop, entity->tank_com_m, entity->center_of_mass_m);
  entity->inertia = inertia;
}
bool all_finite(const std::array<double, 3>& values) {
  return std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]);
}

}  // namespace

World::World(std::size_t retained_snapshots) : store_(retained_snapshots) {}

void World::set_environment(const EnvironmentSpec& spec) {
  environment_ = spec;
}

void World::sort_entities() {
  std::sort(entities_.begin(), entities_.end(),
            [](const WorldEntity& left, const WorldEntity& right) { return left.id < right.id; });
}

bool World::add_entity(WorldEntity entity, std::string* error) {
  if (entity.id.empty()) {
    if (error != nullptr) {
      *error = "entity id must not be empty";
    }
    return false;
  }
  if (find(entity.id) != nullptr) {
    if (error != nullptr) {
      *error = "duplicate entity id '" + entity.id + "'";
    }
    return false;
  }
  entities_.push_back(std::move(entity));
  sort_entities();
  return true;
}

void World::push_command(WorldCommand command) {
  pending_.push_back(std::move(command));
}

const WorldEntity* World::find(std::string_view id) const {
  const auto it = std::lower_bound(
      entities_.begin(), entities_.end(), id,
      [](const WorldEntity& entity, std::string_view value) { return entity.id < value; });
  if (it == entities_.end() || it->id != id) {
    return nullptr;
  }
  return &*it;
}

World::SnapshotHeader World::decode_header(const SnapshotStore::Bytes& bytes) {
  SnapshotHeader header{};
  if (bytes.size() < kSnapshotHeaderBytes) {
    return header;
  }
  const char* magic = "@SRSP";
  if (std::memcmp(bytes.data(), magic, 4) != 0) {
    return header;
  }
  std::memcpy(&header.version, bytes.data() + 4, sizeof(std::uint32_t));
  std::memcpy(&header.stride, bytes.data() + 8, sizeof(std::uint32_t));
  std::memcpy(&header.count, bytes.data() + 12, sizeof(std::uint32_t));
  return header;
}

std::vector<std::uint8_t> World::encode(const std::vector<WorldEntity>& entities) {
  std::vector<std::uint8_t> bytes(kSnapshotHeaderBytes + entities.size() * kBytesPerEntity);
  const char* magic = "@SRSP";
  std::memcpy(bytes.data(), magic, 4);
  const auto version = kSnapshotVersion;
  const auto stride = static_cast<std::uint32_t>(kBytesPerEntity);
  const auto count = static_cast<std::uint32_t>(entities.size());
  std::memcpy(bytes.data() + 4, &version, sizeof(std::uint32_t));
  std::memcpy(bytes.data() + 8, &stride, sizeof(std::uint32_t));
  std::memcpy(bytes.data() + 12, &count, sizeof(std::uint32_t));
  std::size_t offset = kSnapshotHeaderBytes;
  for (const auto& entity : entities) {
    std::memset(bytes.data() + offset, 0, 64);
    const std::size_t copy = std::min<std::size_t>(entity.id.size(), 63);
    std::memcpy(bytes.data() + offset, entity.id.data(), copy);
    offset += 64;
    for (double value : entity.position_m) {
      std::memcpy(bytes.data() + offset, &value, sizeof(double));
      offset += sizeof(double);
    }
    for (double value : entity.velocity_m_s) {
      std::memcpy(bytes.data() + offset, &value, sizeof(double));
      offset += sizeof(double);
    }
    const double mass = entity.mass_kg;
    std::memcpy(bytes.data() + offset, &mass, sizeof(double));
    offset += sizeof(double);
    for (double value : entity.attitude) {
      std::memcpy(bytes.data() + offset, &value, sizeof(double));
      offset += sizeof(double);
    }
    for (double value : entity.omega_body_rad_s) {
      std::memcpy(bytes.data() + offset, &value, sizeof(double));
      offset += sizeof(double);
    }
  }
  return bytes;
}

bool World::step(double dt, std::string* error) {
  if (!std::isfinite(dt) || dt < 0.0) {
    if (error != nullptr) {
      *error = "step length must be finite and non-negative";
    }
    return false;
  }

  // Build the next state without touching the committed one.
  std::vector<WorldEntity> next = entities_;
  for (const auto& command : pending_) {
    const auto it = std::find_if(next.begin(), next.end(), [&](const WorldEntity& entity) {
      return entity.id == command.entity;
    });
    if (it == next.end()) {
      if (error != nullptr) {
        *error = "command targets unknown entity '" + command.entity + "'";
      }
      return false;
    }
    if (!all_finite(command.delta_velocity_m_s)) {
      if (error != nullptr) {
        *error = "command carries a non-finite value for '" + command.entity + "'";
      }
      return false;
    }
    if (command.kind == WorldCommandKind::kSetWheelTorque) {
      if (!std::isfinite(command.value)) {
        if (error != nullptr) {
          *error = "wheel torque must be finite for '" + command.entity + "'";
        }
        return false;
      }
      it->wheel_torque_nm = command.value;
      continue;
    }
    if (command.kind == WorldCommandKind::kSetThrust) {
      if (!std::isfinite(command.value) || command.value < 0.0) {
        if (error != nullptr) {
          *error = "thrust must be finite and non-negative for '" + command.entity + "'";
        }
        return false;
      }
      it->thrust_n = command.value;
      continue;
    }
    for (int axis = 0; axis < 3; ++axis) {
      it->velocity_m_s[static_cast<std::size_t>(axis)] +=
          command.delta_velocity_m_s[static_cast<std::size_t>(axis)];
    }
  }

  for (auto& entity : next) {
    if (!all_finite(entity.position_m) || !all_finite(entity.velocity_m_s)) {
      if (error != nullptr) {
        *error = "entity '" + entity.id + "' holds a non-finite state";
      }
      return false;
    }
    if (!environment_.has_value()) {
      const double thrust_acceleration = entity.thrust_n / entity.mass_kg;
      for (int axis = 0; axis < 3; ++axis) {
        const auto index = static_cast<std::size_t>(axis);
        entity.position_m[index] += entity.velocity_m_s[index] * dt;
      }
      entity.velocity_m_s[0] += thrust_acceleration * dt;
    } else {
      // RK4 on (position, velocity): gravity, thrust, solar pressure and drag
      // are all sampled at the four stages.
      const EnvironmentSpec& spec = *environment_;
      const auto acceleration_at = [&](const std::array<double, 3>& p,
                                       const std::array<double, 3>& v) {
        const EnvironmentSample sample = evaluate_environment(spec, p);
        auto acceleration = sample.gravity_m_s2;
        acceleration[0] += entity.thrust_n / entity.mass_kg;
        const Vec3 srp = solar_radiation_pressure_acceleration(sample, entity.area_m2,
                                                               entity.mass_kg, entity.reflectivity);
        const Vec3 drag = atmospheric_drag_acceleration(spec, p, v, entity.area_m2, entity.mass_kg,
                                                        entity.drag_coefficient);
        for (int axis = 0; axis < 3; ++axis) {
          const auto index = static_cast<std::size_t>(axis);
          acceleration[index] += srp[index] + drag[index];
        }
        return acceleration;
      };
      const std::array<double, 3> p0 = entity.position_m;
      const std::array<double, 3> v0 = entity.velocity_m_s;
      const std::array<double, 3> a1 = acceleration_at(p0, v0);
      std::array<double, 3> p2{};
      std::array<double, 3> v2{};
      for (int axis = 0; axis < 3; ++axis) {
        const auto index = static_cast<std::size_t>(axis);
        p2[index] = p0[index] + 0.5 * dt * v0[index];
        v2[index] = v0[index] + 0.5 * dt * a1[index];
      }
      const std::array<double, 3> a2 = acceleration_at(p2, v2);
      std::array<double, 3> p3{};
      std::array<double, 3> v3{};
      for (int axis = 0; axis < 3; ++axis) {
        const auto index = static_cast<std::size_t>(axis);
        p3[index] = p0[index] + 0.5 * dt * v2[index];
        v3[index] = v0[index] + 0.5 * dt * a2[index];
      }
      const std::array<double, 3> a3 = acceleration_at(p3, v3);
      std::array<double, 3> p4{};
      std::array<double, 3> v4{};
      for (int axis = 0; axis < 3; ++axis) {
        const auto index = static_cast<std::size_t>(axis);
        p4[index] = p0[index] + dt * v3[index];
        v4[index] = v0[index] + dt * a3[index];
      }
      const std::array<double, 3> a4 = acceleration_at(p4, v4);
      const double sixth = dt / 6.0;
      for (int axis = 0; axis < 3; ++axis) {
        const auto index = static_cast<std::size_t>(axis);
        entity.position_m[index] =
            p0[index] + sixth * (v0[index] + 2.0 * v2[index] + 2.0 * v3[index] + v4[index]);
        entity.velocity_m_s[index] =
            v0[index] + sixth * (a1[index] + 2.0 * a2[index] + 2.0 * a3[index] + a4[index]);
      }
      // Torque diagnostics: pressure-centre offset crossed with the total
      // aerodynamic and solar force (attitude integration is a later step).
      const EnvironmentSample sample = evaluate_environment(spec, entity.position_m);
      const Vec3 srp = solar_radiation_pressure_acceleration(sample, entity.area_m2, entity.mass_kg,
                                                             entity.reflectivity);
      const Vec3 drag =
          atmospheric_drag_acceleration(spec, entity.position_m, entity.velocity_m_s,
                                        entity.area_m2, entity.mass_kg, entity.drag_coefficient);
      Vec3 force{};
      for (int axis = 0; axis < 3; ++axis) {
        const auto index = static_cast<std::size_t>(axis);
        force[index] = (srp[index] + drag[index]) * entity.mass_kg;
      }
      const std::array<double, 9> rot = rotation_from_quaternion(entity.attitude);
      const auto to_body = [&rot](const std::array<double, 3>& v) {
        return std::array<double, 3>{rot[0] * v[0] + rot[3] * v[1] + rot[6] * v[2],
                                     rot[1] * v[0] + rot[4] * v[1] + rot[7] * v[2],
                                     rot[2] * v[0] + rot[5] * v[1] + rot[8] * v[2]};
      };
      Vec3 pressure_offset = entity.center_of_pressure_m;
      if (entity.aero_cp_arm_m > 0.0) {
        const Vec3 velocity_body = to_body(entity.velocity_m_s);
        const double alpha =
            std::atan2(velocity_body[2], std::max(1e-12, std::abs(velocity_body[0])));
        pressure_offset[0] += entity.aero_cp_arm_m * std::clamp(alpha, -0.5, 0.5);
      }
      std::array<double, 3> tau_body = torque_from_force(to_body(force), pressure_offset);
      const double r_norm = norm(entity.position_m);
      if (r_norm > 0.0 && spec.earth.mu > 0.0) {
        const std::array<double, 3> r_hat{entity.position_m[0] / r_norm,
                                          entity.position_m[1] / r_norm,
                                          entity.position_m[2] / r_norm};
        const std::array<double, 3> r_body = to_body(r_hat);
        const std::array<double, 3> inertia_r = mat_vec(entity.inertia, r_body);
        const std::array<double, 3> gradient = cross3(r_body, inertia_r);
        const double factor = 3.0 * spec.earth.mu / (r_norm * r_norm * r_norm);
        for (int axis = 0; axis < 3; ++axis) {
          tau_body[axis] += factor * gradient[axis];
        }
      }
      if (entity.wheel_torque_nm != 0.0 && entity.wheel_max_momentum_nms > 0.0) {
        const double next = entity.wheel_momentum_nms + entity.wheel_torque_nm * dt;
        const double clamped =
            std::clamp(next, -entity.wheel_max_momentum_nms, entity.wheel_max_momentum_nms);
        const double applied = (clamped - entity.wheel_momentum_nms) / dt;
        entity.wheel_momentum_nms = clamped;
        tau_body[0] -= applied;
      }
      if (entity.wheel_max_momentum_nms > 0.0 && entity.desaturation_torque_nm > 0.0 &&
          entity.propellant_kg > 0.0 &&
          std::abs(entity.wheel_momentum_nms) >= entity.wheel_max_momentum_nms * 0.99) {
        const double delta =
            std::min(std::abs(entity.wheel_momentum_nms), entity.desaturation_torque_nm * dt);
        const double sign = entity.wheel_momentum_nms > 0.0 ? 1.0 : -1.0;
        entity.wheel_momentum_nms -= sign * delta;
        tau_body[0] += sign * delta / dt;
        if (entity.isp_s > 0.0) {
          const double burned = std::min(
              entity.desaturation_torque_nm / (entity.isp_s * 9.80665) * dt, entity.propellant_kg);
          entity.propellant_kg -= burned;
          entity.mass_kg -= burned;
        }
      }
      entity.last_torque_nm = tau_body;
      const std::array<double, 3> inertia_omega = mat_vec(entity.inertia, entity.omega_body_rad_s);
      const std::array<double, 3> gyro = cross3(entity.omega_body_rad_s, inertia_omega);
      const std::array<double, 3> net{tau_body[0] - gyro[0], tau_body[1] - gyro[1],
                                      tau_body[2] - gyro[2]};
      const std::array<double, 3> alpha = solve3(entity.inertia, net);
      for (int axis = 0; axis < 3; ++axis) {
        entity.omega_body_rad_s[axis] += alpha[axis] * dt;
      }
      const std::array<double, 4> omega_quat{
          0.0, entity.omega_body_rad_s[0], entity.omega_body_rad_s[1], entity.omega_body_rad_s[2]};
      const std::array<double, 4> dq = quat_mul(entity.attitude, omega_quat);
      for (int axis = 0; axis < 4; ++axis) {
        entity.attitude[axis] += 0.5 * dq[axis] * dt;
      }
      double quat_norm = 0.0;
      for (double value : entity.attitude) {
        quat_norm += value * value;
      }
      quat_norm = std::sqrt(quat_norm);
      if (quat_norm > 0.0) {
        for (double& value : entity.attitude) {
          value /= quat_norm;
        }
      }
    }
    // Propellant burn: m_dot = F / (Isp * g0), clamped by what is left. The
    // acceleration above uses the mass at the start of the step (small-dt
    // approximation).
    if (entity.thrust_n > 0.0) {
      if (entity.isp_s <= 0.0 || entity.mass_kg <= 0.0) {
        if (error != nullptr) {
          *error = "entity '" + entity.id + "' has thrust but no isp or mass";
        }
        return false;
      }
      constexpr double g0 = 9.80665;
      const double m_dot = entity.thrust_n / (entity.isp_s * g0);
      const double burned = std::min(m_dot * dt, entity.propellant_kg);
      entity.mass_kg -= burned;
      entity.propellant_kg -= burned;
      if (entity.propellant_kg <= 0.0) {
        entity.propellant_kg = 0.0;
        entity.thrust_n = 0.0;
      }
    }
    update_mass_properties(&entity);
    if (!all_finite(entity.position_m)) {
      if (error != nullptr) {
        *error = "entity '" + entity.id + "' produced a non-finite position";
      }
      return false;
    }
  }

  // Commit: publish the snapshot, then swap the state. Nothing above mutated it.
  const std::vector<std::uint8_t> bytes = encode(next);
  store_.publish(step_index_ + 1, bytes.data(), bytes.size());
  entities_ = std::move(next);
  pending_.clear();
  ++step_index_;
  if (on_commit) {
    on_commit(store_.latest_id(), entities_);
  }
  return true;
}

}  // namespace srsp::core
