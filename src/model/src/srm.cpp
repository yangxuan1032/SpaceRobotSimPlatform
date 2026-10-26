/**
 * @file srm.cpp
 * @brief SRM 1.0 JSON reader, writer and structural validation.
 */
#include <srsp/model/srm.hpp>

#include <algorithm>
#include <unordered_map>

#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <unordered_set>

namespace srsp::model {
namespace {

using json = nlohmann::json;

Vec3 read_vec3(const json& node, const char* key, Vec3 fallback) {
  if (!node.contains(key) || !node[key].is_array() || node[key].size() != 3) {
    return fallback;
  }
  Vec3 value{};
  for (std::size_t i = 0; i < 3; ++i) {
    value[i] = node[key][i].get<Scalar>();
  }
  return value;
}

Mat3 read_mat3(const json& node, const char* key) {
  Mat3 value{};
  if (!node.contains(key) || !node[key].is_array() || node[key].size() != 9) {
    return value;
  }
  for (std::size_t i = 0; i < 9; ++i) {
    value[i] = node[key][i].get<Scalar>();
  }
  return value;
}

JointType read_joint_type(const std::string& text) {
  if (text == "continuous") {
    return JointType::kContinuous;
  }
  if (text == "prismatic") {
    return JointType::kPrismatic;
  }
  if (text == "fixed") {
    return JointType::kFixed;
  }
  return JointType::kRevolute;
}

const char* joint_type_name(JointType type) {
  switch (type) {
    case JointType::kContinuous:
      return "continuous";
    case JointType::kPrismatic:
      return "prismatic";
    case JointType::kFixed:
      return "fixed";
    case JointType::kRevolute:
      break;
  }
  return "revolute";
}

ActuatorType read_actuator_type(const std::string& text) {
  if (text == "wheel") {
    return ActuatorType::kWheel;
  }
  if (text == "thruster") {
    return ActuatorType::kThruster;
  }
  return ActuatorType::kTorque;
}

const char* actuator_type_name(ActuatorType type) {
  switch (type) {
    case ActuatorType::kWheel:
      return "wheel";
    case ActuatorType::kThruster:
      return "thruster";
    case ActuatorType::kTorque:
      break;
  }
  return "torque";
}

SensorType read_sensor_type(const std::string& text) {
  if (text == "star_tracker") {
    return SensorType::kStarTracker;
  }
  if (text == "sun_sensor") {
    return SensorType::kSunSensor;
  }
  if (text == "camera") {
    return SensorType::kCamera;
  }
  return SensorType::kImu;
}

const char* sensor_type_name(SensorType type) {
  switch (type) {
    case SensorType::kStarTracker:
      return "star_tracker";
    case SensorType::kSunSensor:
      return "sun_sensor";
    case SensorType::kCamera:
      return "camera";
    case SensorType::kImu:
      break;
  }
  return "imu";
}

}  // namespace

LoadResult load_from_json(const std::string& text) {
  LoadResult result;
  json document;
  try {
    document = json::parse(text);
  } catch (const std::exception& error) {
    result.issues.push_back({"<document>", std::string("invalid JSON: ") + error.what()});
    return result;
  }

  if (!document.is_object()) {
    result.issues.push_back({"<document>", "root must be an object"});
    return result;
  }
  if (document.value("schema", std::string{}) != "srsp.model/1") {
    result.issues.push_back({"schema", "expected \"srsp.model/1\""});
    return result;
  }

  Model& model = result.model;
  model.name = document.value("name", std::string{"unnamed"});
  model.version = document.value("version", std::string{"1.0"});

  if (document.contains("bodies")) {
    for (const auto& node : document["bodies"]) {
      Body body;
      body.id = node.value("id", std::string{});
      body.parent_joint = node.value("parent_joint", std::string{});
      if (node.contains("inertia")) {
        const auto& inertia = node["inertia"];
        body.inertia.mass = inertia.value("mass", 0.0);
        body.inertia.com = read_vec3(inertia, "com", Vec3{});
        body.inertia.inertia = read_mat3(inertia, "tensor");
      }
      if (node.contains("geometry")) {
        for (const auto& visual : node["geometry"]) {
          Geometry geometry;
          geometry.mesh = visual.value("mesh", std::string{});
          geometry.scale = read_vec3(visual, "scale", Vec3{1.0, 1.0, 1.0});
          geometry.collision = visual.value("collision", false);
          body.geometry.push_back(std::move(geometry));
        }
      }
      model.bodies.push_back(std::move(body));
    }
  }

  if (document.contains("joints")) {
    for (const auto& node : document["joints"]) {
      Joint joint;
      joint.id = node.value("id", std::string{});
      joint.parent_body = node.value("parent", std::string{});
      joint.child_body = node.value("child", std::string{});
      joint.type = read_joint_type(node.value("type", std::string{"revolute"}));
      joint.axis = read_vec3(node, "axis", Vec3{0.0, 0.0, 1.0});
      joint.lower = node.value("lower", 0.0);
      joint.upper = node.value("upper", 0.0);
      joint.origin = read_vec3(node, "origin", Vec3{});
      joint.origin_rpy = read_vec3(node, "origin_rpy", Vec3{});
      model.joints.push_back(std::move(joint));
    }
  }

  if (document.contains("actuators")) {
    for (const auto& node : document["actuators"]) {
      Actuator actuator;
      actuator.id = node.value("id", std::string{});
      actuator.type = read_actuator_type(node.value("type", std::string{"torque"}));
      actuator.joint = node.value("joint", std::string{});
      actuator.body = node.value("body", std::string{});
      actuator.axis = read_vec3(node, "axis", Vec3{1.0, 0.0, 0.0});
      actuator.max_effort = node.value("max_effort", 0.0);
      model.actuators.push_back(std::move(actuator));
    }
  }

  if (document.contains("sensors")) {
    for (const auto& node : document["sensors"]) {
      Sensor sensor;
      sensor.id = node.value("id", std::string{});
      sensor.type = read_sensor_type(node.value("type", std::string{"imu"}));
      sensor.body = node.value("body", std::string{});
      sensor.rate_hz = node.value("rate_hz", 0.0);
      sensor.mount_position = read_vec3(node, "mount_position", Vec3{});
      if (node.contains("mount_quaternion") && node["mount_quaternion"].size() == 4) {
        for (std::size_t i = 0; i < 4; ++i) {
          sensor.mount_quaternion[i] = node["mount_quaternion"][i].get<Scalar>();
        }
      }
      model.sensors.push_back(std::move(sensor));
    }
  }

  result.issues = validate(model);
  return result;
}

LoadResult load_from_file(const std::string& path) {
  LoadResult result;
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    result.issues.push_back({path, "cannot open model file"});
    return result;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return load_from_json(buffer.str());
}

std::string to_json(const Model& model) {
  json document;
  document["schema"] = "srsp.model/1";
  document["name"] = model.name;
  document["version"] = model.version;

  json bodies = json::array();
  for (const auto& body : model.bodies) {
    json node;
    node["id"] = body.id;
    if (!body.parent_joint.empty()) {
      node["parent_joint"] = body.parent_joint;
    }
    node["inertia"] = {
        {"mass", body.inertia.mass},
        {"com", body.inertia.com},
        {"tensor", body.inertia.inertia},
    };
    json geometry = json::array();
    for (const auto& item : body.geometry) {
      geometry.push_back(
          {{"mesh", item.mesh}, {"scale", item.scale}, {"collision", item.collision}});
    }
    node["geometry"] = std::move(geometry);
    bodies.push_back(std::move(node));
  }
  document["bodies"] = std::move(bodies);

  json joints = json::array();
  for (const auto& joint : model.joints) {
    joints.push_back({{"id", joint.id},
                      {"parent", joint.parent_body},
                      {"child", joint.child_body},
                      {"type", joint_type_name(joint.type)},
                      {"axis", joint.axis},
                      {"lower", joint.lower},
                      {"upper", joint.upper},
                      {"origin", joint.origin},
                      {"origin_rpy", joint.origin_rpy}});
  }
  document["joints"] = std::move(joints);

  json actuators = json::array();
  for (const auto& actuator : model.actuators) {
    actuators.push_back({{"id", actuator.id},
                         {"type", actuator_type_name(actuator.type)},
                         {"joint", actuator.joint},
                         {"body", actuator.body},
                         {"axis", actuator.axis},
                         {"max_effort", actuator.max_effort}});
  }
  document["actuators"] = std::move(actuators);

  json sensors = json::array();
  for (const auto& sensor : model.sensors) {
    sensors.push_back({{"id", sensor.id},
                       {"type", sensor_type_name(sensor.type)},
                       {"body", sensor.body},
                       {"rate_hz", sensor.rate_hz},
                       {"mount_position", sensor.mount_position},
                       {"mount_quaternion", sensor.mount_quaternion}});
  }
  document["sensors"] = std::move(sensors);

  return document.dump(2);
}

std::vector<ValidationIssue> validate(const Model& model) {
  std::vector<ValidationIssue> issues;
  if (model.bodies.empty()) {
    issues.push_back({"bodies", "model has no bodies"});
    return issues;
  }

  std::unordered_map<std::string, const Body*> bodies;
  for (const auto& body : model.bodies) {
    if (body.id.empty()) {
      issues.push_back({"bodies", "body has an empty id"});
      continue;
    }
    if (!bodies.emplace(body.id, &body).second) {
      issues.push_back({body.id, "duplicate body id"});
    }
  }

  std::unordered_map<std::string, const Joint*> joints;
  for (const auto& joint : model.joints) {
    if (joint.id.empty()) {
      issues.push_back({"joints", "joint has an empty id"});
      continue;
    }
    if (!joints.emplace(joint.id, &joint).second) {
      issues.push_back({joint.id, "duplicate joint id"});
      continue;
    }
    if (bodies.find(joint.parent_body) == bodies.end()) {
      issues.push_back({joint.id, "parent body does not exist"});
    }
    if (bodies.find(joint.child_body) == bodies.end()) {
      issues.push_back({joint.id, "child body does not exist"});
    }
    if (joint.type != JointType::kFixed && joint.type != JointType::kContinuous &&
        !(joint.lower < joint.upper)) {
      issues.push_back({joint.id, "joint limits must satisfy lower < upper"});
    }
    const Scalar axis_norm = joint.axis[0] * joint.axis[0] + joint.axis[1] * joint.axis[1] +
                             joint.axis[2] * joint.axis[2];
    if (axis_norm < 1e-12) {
      issues.push_back({joint.id, "joint axis is degenerate"});
    }
  }

  // Every body except the roots must be the child of exactly one joint.
  std::unordered_map<std::string, int> incoming;
  for (const auto& joint : model.joints) {
    ++incoming[joint.child_body];
  }
  int roots = 0;
  for (const auto& body : model.bodies) {
    const int count = incoming[body.id];
    if (count == 0) {
      ++roots;
      if (!body.parent_joint.empty()) {
        issues.push_back({body.id, "body declares a parent joint but has no joint"});
      }
    } else if (count > 1) {
      issues.push_back({body.id, "body has more than one incoming joint"});
    }
  }
  if (roots == 0) {
    issues.push_back({"bodies", "model has no root body (cycle or empty tree)"});
  }

  // Cycle detection: walk parents from every body with a depth cap.
  for (const auto& body : model.bodies) {
    std::string current = body.id;
    std::unordered_set<std::string> seen;
    for (std::size_t depth = 0; depth <= model.bodies.size(); ++depth) {
      if (!seen.insert(current).second) {
        issues.push_back({body.id, "kinematic cycle detected"});
        break;
      }
      std::string next;
      for (const auto& joint : model.joints) {
        if (joint.child_body == current) {
          next = joint.parent_body;
          break;
        }
      }
      if (next.empty()) {
        break;
      }
      current = next;
    }
  }

  for (const auto& body : model.bodies) {
    if (body.inertia.mass < 0.0) {
      issues.push_back({body.id, "mass must not be negative"});
    }
    const Mat3& tensor = body.inertia.inertia;
    const bool symmetric = std::abs(tensor[1] - tensor[3]) < 1e-9 &&
                           std::abs(tensor[2] - tensor[6]) < 1e-9 &&
                           std::abs(tensor[5] - tensor[7]) < 1e-9;
    if (!symmetric) {
      issues.push_back({body.id, "inertia tensor must be symmetric"});
    }
  }

  std::unordered_set<std::string> actuator_ids;
  for (const auto& actuator : model.actuators) {
    if (!actuator_ids.insert(actuator.id).second) {
      issues.push_back({actuator.id, "duplicate actuator id"});
    }
    if (!actuator.joint.empty() && joints.find(actuator.joint) == joints.end()) {
      issues.push_back({actuator.id, "actuator references an unknown joint"});
    }
    if (!actuator.body.empty() && bodies.find(actuator.body) == bodies.end()) {
      issues.push_back({actuator.id, "actuator references an unknown body"});
    }
    if (actuator.max_effort < 0.0) {
      issues.push_back({actuator.id, "max_effort must not be negative"});
    }
  }

  std::unordered_set<std::string> sensor_ids;
  for (const auto& sensor : model.sensors) {
    if (!sensor_ids.insert(sensor.id).second) {
      issues.push_back({sensor.id, "duplicate sensor id"});
    }
    if (bodies.find(sensor.body) == bodies.end()) {
      issues.push_back({sensor.id, "sensor references an unknown body"});
    }
    if (sensor.rate_hz < 0.0) {
      issues.push_back({sensor.id, "rate_hz must not be negative"});
    }
  }

  return issues;
}

}  // namespace srsp::model
