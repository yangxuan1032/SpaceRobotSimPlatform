/**
 * @file srm.hpp
 * @brief SRM 1.0: the spatial robot model intermediate representation.
 * @brief_zh SRM 1.0：空间机器人模型的中间表示。
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
/** @brief_zh 模型层使用的标量类型（SI 单位）。 */
using Scalar = double;

/** @brief 3-vector. */
/** @brief_zh 三维向量。 */
using Vec3 = std::array<Scalar, 3>;

/** @brief Row-major 3x3 matrix. */
/** @brief_zh 行主序 3×3 矩阵。 */
using Mat3 = std::array<Scalar, 9>;

/** @brief Rigid-body inertia expressed at the body frame origin. */
/** @brief_zh 以体原点表达的刚体惯量。 */
struct Inertia {
  Scalar mass = 0.0;  ///< Mass in kg (> 0 for dynamic bodies). / 质量（kg，动力学体需 > 0）。
  Vec3 com{};         ///< Centre of mass in the body frame, m. / 体质心位置（米）。
  Mat3 inertia{};     ///< Inertia tensor about the COM, kg*m^2. / 绕质心的惯量张量（kg·m²）。
};

/** @brief Joint kind between a parent and a child body. */
/** @brief_zh 父子体之间的关节类型。 */
enum class JointType : std::int32_t {
  kRevolute = 0,    ///< Rotation about an axis, limited. / 绕轴转动，有限位。
  kContinuous = 1,  ///< Rotation about an axis, unlimited. / 绕轴转动，无限位。
  kPrismatic = 2,   ///< Translation along an axis, limited. / 沿轴平移，有限位。
  kFixed = 3,       ///< Rigid attachment. / 刚性连接。
};

/** @brief One joint of the kinematic tree. */
/** @brief_zh 运动学树中的一个关节。 */
struct Joint {
  std::string id;           ///< Unique within the model. / 模型内唯一。
  std::string parent_body;  ///< Parent body id. / 父体 id。
  std::string child_body;   ///< Child body id. / 子体 id。
  JointType type = JointType::kRevolute;
  Vec3 axis{0.0, 0.0, 1.0};  ///< Unit axis in the joint frame. / 关节坐标系下的单位轴。
  Scalar lower =
      0.0;  ///< Lower limit (rad or m); ignored for fixed. / 下限（rad 或 m），固定关节忽略。
  Scalar upper =
      0.0;        ///< Upper limit (rad or m); ignored for fixed. / 上限（rad 或 m），固定关节忽略。
  Vec3 origin{};  ///< Joint frame offset in the parent body, m. / 关节坐标系在父体中的偏移（米）。
  Vec3 origin_rpy{};  ///< Joint frame rotation (roll, pitch, yaw), rad. /
                      ///< 关节坐标系旋转（rpy，弧度）。
};

/** @brief Visual or collision geometry attached to a body. */
/** @brief_zh 挂在体上的可视化或碰撞几何。 */
struct Geometry {
  std::string mesh;           ///< Relative path to a glTF/OBJ file. / glTF/OBJ 相对路径。
  Vec3 scale{1.0, 1.0, 1.0};  ///< Uniform or per-axis scale. / 统一或分轴缩放。
  bool collision = false;     ///< True for collision geometry. / 为真表示碰撞几何。
};

/** @brief One rigid body. */
/** @brief_zh 一个刚体。 */
struct Body {
  std::string id;                  ///< Unique within the model.
  std::string parent_joint;        ///< Incoming joint; empty for the base. / 入关节；基座为空。
  Inertia inertia;                 ///< Mass properties. / 质量属性。
  std::vector<Geometry> geometry;  ///< Visual and collision shapes. / 可视化与碰撞形状。
};

/** @brief Actuator kind. */
/** @brief_zh 执行器类型。 */
enum class ActuatorType : std::int32_t {
  kTorque = 0,    ///< Direct joint torque or force. / 直接关节力矩或力。
  kWheel = 1,     ///< Reaction wheel: torque about its spin axis. / 反作用轮：绕自旋轴的力矩。
  kThruster = 2,  ///< Body-fixed force along an axis. / 体固连沿轴的推力。
};

/** @brief One actuator bound to a joint or a body. */
/** @brief_zh 绑定到关节或体的一个执行器。 */
struct Actuator {
  std::string id;  ///< Unique within the model. / 模型内唯一。
  ActuatorType type = ActuatorType::kTorque;
  std::string joint;         ///< Joint id (torque/wheel) or empty. / 关节 id（力矩/飞轮）或空。
  std::string body;          ///< Body id for thrusters. / 推力器所属体 id。
  Vec3 axis{1.0, 0.0, 0.0};  ///< Spin or thrust axis, body frame. / 自旋或推力轴（体系）。
  Scalar max_effort = 0.0;   ///< Saturation, N*m or N. / 饱和值（N·m 或 N）。
};

/** @brief Sensor kinds supported by the model. */
/** @brief_zh 模型支持的传感器类型。 */
enum class SensorType : std::int32_t {
  kImu = 0,
  kStarTracker = 1,
  kSunSensor = 2,
  kCamera = 3,
};

/** @brief One sensor mounted on a body. */
/** @brief_zh 安装在体上的一个传感器。 */
struct Sensor {
  std::string id;  ///< Unique within the model. / 模型内唯一。
  SensorType type = SensorType::kImu;
  std::string body;       ///< Host body id. / 宿主体 id。
  Scalar rate_hz = 0.0;   ///< Sampling rate. / 采样率（Hz）。
  Vec3 mount_position{};  ///< Offset in the body frame, m. / 体系中的安装偏移（米）。
  std::array<Scalar, 4> mount_quaternion{0.0, 0.0, 0.0, 1.0};  ///< x,y,z,w. / 安装四元数。
};

/** @brief Loaded SRM model. */
/** @brief_zh 已加载的 SRM 模型。 */
struct Model {
  std::string name;                 ///< Human-readable model name. / 可读模型名。
  std::string version = "1.0";      ///< SRM version string. / SRM 版本串。
  std::vector<Body> bodies;         ///< Bodies, base first by convention. / 体列表，约定基座在前。
  std::vector<Joint> joints;        ///< Joints, parents before children. / 关节列表，父先于子。
  std::vector<Actuator> actuators;  ///< Actuators. / 执行器。
  std::vector<Sensor> sensors;      ///< Sensors. / 传感器。
};

/** @brief Validation issue found while loading or checking a model. */
/** @brief_zh 加载或校验模型时发现的问题。 */
struct ValidationIssue {
  std::string
      where;  ///< Identifier or JSON path of the offending element. / 出错元素的标识或 JSON 路径。
  std::string message;  ///< Human-readable description. / 可读描述。
};

/** @brief Result of loading a model. */
/** @brief_zh 模型加载结果。 */
struct LoadResult {
  Model model;  ///< Parsed model (may be partial). / 解析出的模型（可能不完整）。
  std::vector<ValidationIssue> issues;  ///< Empty when the model is valid. / 模型合法时为空。
  [[nodiscard]] bool ok() const noexcept { return issues.empty(); }
};

/**
 * @brief Parses an SRM document from JSON text.
 * @brief_zh 从 JSON 文本解析 SRM 文档。
 * @param json UTF-8 JSON document following `srsp.model/1`.
 * @return Parsed model plus validation issues; structural problems are
 *         reported as issues rather than exceptions.
 */
LoadResult load_from_json(const std::string& json);

/** @brief Loads a model from a file; unreadable files yield one issue. */
/** @brief_zh 从文件加载模型；文件不可读时返回一条问题。 */
LoadResult load_from_file(const std::string& path);

/** @brief Serialises a model back to JSON (stable field order). */
/** @brief_zh 将模型序列化回 JSON（字段顺序稳定）。 */
std::string to_json(const Model& model);

/**
 * @brief Validates structural rules: unique ids, acyclic tree, inertia sanity,
 * @brief_zh 校验结构规则：id 唯一、树无环、惯量合理。
 * joint limits and reference integrity.
 */
std::vector<ValidationIssue> validate(const Model& model);

}  // namespace srsp::model

#endif  // SRSP_MODEL_SRM_HPP
