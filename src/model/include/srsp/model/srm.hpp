/**
 * @file srm.hpp
 * @brief SRM 1.0: the spatial robot model intermediate representation.
 *
 * SRM is the single source of truth for kinematics, inertia, actuation and
 * sensors. External formats (URDF/SDF/MJCF) are imported into SRM; the kernel
 * never reads them directly.
 *
 * Units follow docs/03: metre, kilogram, second, radian, newton, SI throughout.
 */
#ifndef SRSP_MODEL_SRM_HPP
#define SRSP_MODEL_SRM_HPP

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace srsp::model {

/** @brief Scalar type used by the model layer (SI units). */
using Scalar = double;

/** @brief 3-vector. */
using Vec3 = std::array<Scalar, 3>;

/** @brief Row-major 3x3 matrix. */
using Mat3 = std::array<Scalar, 9>;

/** @brief Rigid-body inertia expressed at the body frame origin. */
struct Inertia {
  Scalar mass = 0.0;  ///< Mass in kg (> 0 for dynamic bodies).
  Vec3 com{};         ///< Centre of mass in the body frame, m.
  Mat3 inertia{};     ///< Inertia tensor about the COM, kg*m^2.
};

/** @brief Joint kind between a parent and a child body. */
enum class JointType : std::int32_t {
  kRevolute = 0,    ///< Rotation about an axis, limited.
  kContinuous = 1,  ///< Rotation about an axis, unlimited.
  kPrismatic = 2,   ///< Translation along an axis, limited.
  kFixed = 3,       ///< Rigid attachment.
};

/** @brief One joint of the kinematic tree. */
struct Joint {
  std::string id;           ///< Unique within the model.
  std::string parent_body;  ///< Parent body id.
  std::string child_body;   ///< Child body id.
  JointType type = JointType::kRevolute;
  Vec3 axis{0.0, 0.0, 1.0};  ///< Unit axis in the joint frame.
  Scalar lower = 0.0;        ///< Lower limit (rad or m); ignored for fixed.
  Scalar upper = 0.0;        ///< Upper limit (rad or m); ignored for fixed.
  Vec3 origin{};             ///< Joint frame offset in the parent body, m.
  Vec3 origin_rpy{};         ///< Joint frame rotation (roll, pitch, yaw), rad.
};

/** @brief Visual or collision geometry attached to a body. */
struct Geometry {
  std::string mesh;           ///< Relative path to a glTF/OBJ file.
  Vec3 scale{1.0, 1.0, 1.0};  ///< Uniform or per-axis scale.
  bool collision = false;     ///< True for collision geometry.
};

/** @brief One rigid body. */
struct Body {
  std::string id;                  ///< Unique within the model.
  std::string parent_joint;        ///< Incoming joint; empty for the base.
  Inertia inertia;                 ///< Mass properties.
  std::vector<Geometry> geometry;  ///< Visual and collision shapes.
};

/** @brief Actuator kind. */
enum class ActuatorType : std::int32_t {
  kTorque = 0,    ///< Direct joint torque or force.
  kWheel = 1,     ///< Reaction wheel: torque about its spin axis.
  kThruster = 2,  ///< Body-fixed force along an axis.
};

/** @brief One actuator bound to a joint or a body. */
struct Actuator {
  std::string id;  ///< Unique within the model.
  ActuatorType type = ActuatorType::kTorque;
  std::string joint;         ///< Joint id (torque/wheel) or empty.
  std::string body;          ///< Body id for thrusters.
  Vec3 axis{1.0, 0.0, 0.0};  ///< Spin or thrust axis, body frame.
  Scalar max_effort = 0.0;   ///< Saturation, N*m or N.
};

/** @brief Sensor kinds supported by the model. */
enum class SensorType : std::int32_t {
  kImu = 0,
  kStarTracker = 1,
  kSunSensor = 2,
  kCamera = 3,
};

/** @brief One sensor mounted on a body. */
struct Sensor {
  std::string id;  ///< Unique within the model.
  SensorType type = SensorType::kImu;
  std::string body;                                            ///< Host body id.
  Scalar rate_hz = 0.0;                                        ///< Sampling rate.
  Vec3 mount_position{};                                       ///< Offset in the body frame, m.
  std::array<Scalar, 4> mount_quaternion{0.0, 0.0, 0.0, 1.0};  ///< x,y,z,w.
};

/** @brief Loaded SRM model. */
struct Model {
  std::string name;                 ///< Human-readable model name.
  std::string version = "1.0";      ///< SRM version string.
  std::vector<Body> bodies;         ///< Bodies, base first by convention.
  std::vector<Joint> joints;        ///< Joints, parents before children.
  std::vector<Actuator> actuators;  ///< Actuators.
  std::vector<Sensor> sensors;      ///< Sensors.
};

/** @brief Validation issue found while loading or checking a model. */
struct ValidationIssue {
  std::string where;    ///< Identifier or JSON path of the offending element.
  std::string message;  ///< Human-readable description.
};

/** @brief Result of loading a model. */
struct LoadResult {
  Model model;                          ///< Parsed model (may be partial).
  std::vector<ValidationIssue> issues;  ///< Empty when the model is valid.
  [[nodiscard]] bool ok() const noexcept { return issues.empty(); }
};

/**
 * @brief Parses an SRM document from JSON text.
 * @param json UTF-8 JSON document following `srsp.model/1`.
 * @return Parsed model plus validation issues; structural problems are
 *         reported as issues rather than exceptions.
 */
LoadResult load_from_json(const std::string& json);

/** @brief Loads a model from a file; unreadable files yield one issue. */
LoadResult load_from_file(const std::string& path);

/** @brief Serialises a model back to JSON (stable field order). */
std::string to_json(const Model& model);

/**
 * @brief Validates structural rules: unique ids, acyclic tree, inertia sanity,
 * joint limits and reference integrity.
 */
std::vector<ValidationIssue> validate(const Model& model);

}  // namespace srsp::model

#endif  // SRSP_MODEL_SRM_HPP
