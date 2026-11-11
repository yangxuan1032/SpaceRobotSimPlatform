/**
 * @file environment.cpp
 * @brief Implementation of environment sampling and RK4 orbit propagation (W4).
 */
#include <srsp/core/environment.hpp>

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

namespace srsp::core {

namespace {

Vec3 sub(const Vec3& a, const Vec3& b) {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
Vec3 add(const Vec3& a, const Vec3& b) {
  return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}
Vec3 scale(const Vec3& a, double s) {
  return {a[0] * s, a[1] * s, a[2] * s};
}
double dot(const Vec3& a, const Vec3& b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 normalized(const Vec3& value) {
  const double length = norm(value);
  if (length <= 0.0) {
    return {1.0, 0.0, 0.0};
  }
  return scale(value, 1.0 / length);
}
bool finite(const Vec3& value) {
  return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

Vec3 central_gravity(double mu, const Vec3& position) {
  const double r = norm(position);
  if (r <= 0.0 || mu == 0.0) {
    return {};
  }
  const double factor = -mu / (r * r * r);
  return scale(position, factor);
}

/** J2 acceleration in the body-fixed z frame, evaluated in inertial axes. */
Vec3 j2_acceleration(const CelestialBody& body, const Vec3& position) {
  if (body.j2 == 0.0 || body.reference_radius_m <= 0.0) {
    return {};
  }
  const double r = norm(position);
  if (r <= 0.0) {
    return {};
  }
  const double z = position[2] / r;
  const double factor = 1.5 * body.j2 * body.mu * body.reference_radius_m *
                        body.reference_radius_m / (r * r * r * r * r);
  return {factor * position[0] * (5.0 * z * z - 1.0), factor * position[1] * (5.0 * z * z - 1.0),
          factor * position[2] * (5.0 * z * z - 3.0)};
}

/** Differential third-body acceleration (direct minus indirect term). */
Vec3 third_body_acceleration(const CelestialBody& body, const Vec3& position) {
  if (body.mu == 0.0) {
    return {};
  }
  const Vec3 to_body = sub(body.position_m, position);
  const double distance = norm(to_body);
  const double body_distance = norm(body.position_m);
  if (distance <= 0.0 || body_distance <= 0.0) {
    return {};
  }
  const Vec3 direct = scale(to_body, body.mu / (distance * distance * distance));
  const Vec3 indirect =
      scale(body.position_m, body.mu / (body_distance * body_distance * body_distance));
  return sub(direct, indirect);
}

Vec3 total_acceleration(const EnvironmentSpec& spec, const Vec3& position) {
  Vec3 acceleration = central_gravity(spec.earth.mu, position);
  if (spec.use_j2) {
    acceleration = add(acceleration, j2_acceleration(spec.earth, position));
  }
  if (spec.use_third_body) {
    acceleration = add(acceleration, third_body_acceleration(spec.sun, position));
    acceleration = add(acceleration, third_body_acceleration(spec.moon, position));
  }
  return acceleration;
}

/** Fraction of the solar disc visible from @p position, 0 or 1 (umu/ lit). */
double visibility_from_shadow(const EnvironmentSpec& spec, const Vec3& position) {
  const Vec3 to_sun = sub(spec.sun.position_m, position);
  const double sun_distance = norm(to_sun);
  if (sun_distance <= 0.0) {
    return 1.0;
  }
  const Vec3 sun_direction = scale(to_sun, 1.0 / sun_distance);
  const Vec3 to_earth = sub(spec.earth.position_m, position);
  const double along = dot(to_earth, sun_direction);
  if (along <= 0.0) {
    return 1.0;  // Earth is behind the entity relative to the sun.
  }
  const double perpendicular = norm(sub(to_earth, scale(sun_direction, along)));
  if (perpendicular >= spec.earth.radius_m) {
    return 1.0;  // Outside the umbra cylinder.
  }
  // Approximate penumbra: fully dark when the entity is behind the Earth and
  // inside its angular radius as seen from the sun.
  const double sun_to_earth = norm(sub(spec.earth.position_m, spec.sun.position_m));
  const double umbra = std::atan2(spec.earth.radius_m, sun_to_earth);
  const double offset = std::atan2(perpendicular, along);
  return offset < umbra ? 0.0 : 1.0;
}

}  // namespace

double norm(const Vec3& value) {
  return std::sqrt(dot(value, value));
}

double circular_speed(double mu, double r) {
  return r > 0.0 ? std::sqrt(mu / r) : 0.0;
}

double orbital_period(double mu, double r) {
  return r > 0.0 && mu > 0.0 ? 2.0 * 3.14159265358979323846 * std::sqrt(r * r * r / mu) : 0.0;
}

EnvironmentSample evaluate_environment(const EnvironmentSpec& spec, const Vec3& entity_position) {
  EnvironmentSample sample;
  sample.sun_direction = normalized(sub(spec.sun.position_m, entity_position));
  const double distance = norm(sub(spec.sun.position_m, entity_position));
  const double au = 1.495978707e11;
  if (distance > 0.0) {
    const double ratio = au / distance;
    sample.solar_flux_w_m2 = spec.solar_flux_w_m2 * ratio * ratio;
  }
  sample.visibility = visibility_from_shadow(spec, entity_position);
  sample.eclipse = sample.visibility == 0.0;
  if (spec.atmosphere.enabled) {
    const double altitude = norm(entity_position) - spec.earth.radius_m;
    sample.density_kg_m3 =
        spec.atmosphere.rho0_kg_m3 * std::exp(-(altitude - spec.atmosphere.reference_altitude_m) /
                                              spec.atmosphere.scale_height_m);
  }
  sample.gravity_m_s2 = total_acceleration(spec, entity_position);
  return sample;
}

bool propagate_orbit(const EnvironmentSpec& spec, OrbitState* state, double dt) {
  if (state == nullptr || !std::isfinite(dt) || dt <= 0.0) {
    return false;
  }
  // Classical RK4 on (position, velocity) with a state-dependent acceleration.
  const auto derivative = [&spec](const OrbitState& current) {
    OrbitState slope;
    slope.position_m = current.velocity_m_s;
    slope.velocity_m_s = total_acceleration(spec, current.position_m);
    return slope;
  };
  const auto advance = [](const OrbitState& base, const OrbitState& slope, double h) {
    OrbitState next;
    next.position_m = add(base.position_m, scale(slope.position_m, h));
    next.velocity_m_s = add(base.velocity_m_s, scale(slope.velocity_m_s, h));
    return next;
  };

  const OrbitState k1 = derivative(*state);
  const OrbitState k2 = derivative(advance(*state, k1, 0.5 * dt));
  const OrbitState k3 = derivative(advance(*state, k2, 0.5 * dt));
  const OrbitState k4 = derivative(advance(*state, k3, dt));
  const double sixth = dt / 6.0;
  for (std::size_t axis = 0; axis < 3; ++axis) {
    state->position_m[axis] += sixth * (k1.position_m[axis] + 2.0 * k2.position_m[axis] +
                                        2.0 * k3.position_m[axis] + k4.position_m[axis]);
    state->velocity_m_s[axis] += sixth * (k1.velocity_m_s[axis] + 2.0 * k2.velocity_m_s[axis] +
                                          2.0 * k3.velocity_m_s[axis] + k4.velocity_m_s[axis]);
  }
  return finite(state->position_m) && finite(state->velocity_m_s);
}

bool celestial_from_manifest(const std::string& json_text, CelestialBody* body,
                             std::string* error) {
  if (body == nullptr) {
    if (error != nullptr) {
      *error = "celestial body output is null";
    }
    return false;
  }
  nlohmann::json document;
  try {
    document = nlohmann::json::parse(json_text);
  } catch (const nlohmann::json::exception& parse_error) {
    if (error != nullptr) {
      *error = std::string("invalid JSON: ") + parse_error.what();
    }
    return false;
  }
  if (!document.is_object() || !document.contains("kind") || !document["kind"].is_string() ||
      document["kind"] != "env.celestial") {
    if (error != nullptr) {
      *error = "expected kind \"env.celestial\"";
    }
    return false;
  }
  double mu = 0.0;
  double radius = 0.0;
  double j2 = 0.0;
  double reference = 0.0;
  Vec3 position{};
  const auto& properties = document.contains("params") && document["params"].contains("properties")
                               ? document["params"]["properties"]
                               : nlohmann::json::object();
  const auto value_of = [&](const char* name, double fallback) {
    if (!properties.contains(name) || !properties[name].is_object()) {
      return fallback;
    }
    const auto& spec = properties[name];
    const auto it = spec.contains("value")
                        ? spec["value"]
                        : (spec.contains("default") ? spec["default"] : nlohmann::json());
    return it.is_number() ? it.get<double>() : fallback;
  };
  mu = value_of("mu", 0.0);
  radius = value_of("radius_m", 0.0);
  j2 = value_of("j2", 0.0);
  reference = value_of("reference_radius_m", 0.0);
  if (properties.contains("ephemeris") && properties["ephemeris"].is_object() &&
      properties["ephemeris"].contains("default") &&
      properties["ephemeris"]["default"].is_string()) {
    const std::string text = properties["ephemeris"]["default"].get<std::string>();
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (std::sscanf(text.c_str(), "fixed:%lf,%lf,%lf", &x, &y, &z) == 3) {
      position = {x, y, z};
    }
  }
  if (mu <= 0.0) {
    if (error != nullptr) {
      *error = "celestial manifest is missing a positive mu";
    }
    return false;
  }
  if (radius <= 0.0) {
    if (error != nullptr) {
      *error = "celestial manifest is missing a positive radius_m";
    }
    return false;
  }
  body->id = document.value("id", "unknown");
  body->mu = mu;
  body->radius_m = radius;
  body->j2 = j2;
  body->reference_radius_m = reference;
  body->position_m = position;
  return true;
}

Vec3 solar_radiation_pressure_acceleration(const EnvironmentSample& sample, double area_m2,
                                           double mass_kg, double reflectivity) {
  if (area_m2 <= 0.0 || mass_kg <= 0.0 || sample.visibility <= 0.0) {
    return {};
  }
  constexpr double c = 2.99792458e8;
  const double pressure = sample.solar_flux_w_m2 / c;  // N/m^2
  const double magnitude = reflectivity * area_m2 / mass_kg * pressure;
  return scale(sample.sun_direction, -magnitude);
}

Vec3 gravity_gradient_torque(double mu, const Vec3& position,
                             const std::array<double, 9>& inertia) {
  const double r = norm(position);
  if (r <= 0.0 || mu == 0.0) {
    return {};
  }
  const Vec3 r_hat = scale(position, 1.0 / r);
  const Vec3 inertia_r{inertia[0] * r_hat[0] + inertia[1] * r_hat[1] + inertia[2] * r_hat[2],
                       inertia[3] * r_hat[0] + inertia[4] * r_hat[1] + inertia[5] * r_hat[2],
                       inertia[6] * r_hat[0] + inertia[7] * r_hat[1] + inertia[8] * r_hat[2]};
  const double factor = 3.0 * mu / (r * r * r);
  return scale(cross(r_hat, inertia_r), factor);
}

Vec3 torque_from_force(const Vec3& force, const Vec3& offset_m) {
  return cross(offset_m, force);
}

Vec3 atmospheric_drag_acceleration(const EnvironmentSpec& spec, const Vec3& position,
                                   const Vec3& velocity, double area_m2, double mass_kg,
                                   double drag_coefficient) {
  if (!spec.atmosphere.enabled || area_m2 <= 0.0 || mass_kg <= 0.0) {
    return {};
  }
  const double altitude = norm(position) - spec.earth.radius_m;
  const double density =
      spec.atmosphere.rho0_kg_m3 *
      std::exp(-(altitude - spec.atmosphere.reference_altitude_m) / spec.atmosphere.scale_height_m);
  if (!(density > 0.0)) {
    return {};
  }
  const double speed = norm(velocity);
  if (speed <= 0.0) {
    return {};
  }
  const double factor = -0.5 * density * drag_coefficient * area_m2 / mass_kg * speed;
  return scale(velocity, factor);
}
}  // namespace srsp::core
