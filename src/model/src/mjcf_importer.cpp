/**
 * @file mjcf_importer.cpp
 * @brief MJCF parsing (tinyxml2) and conversion into SRM.
 */
#include <srsp/model/mjcf_importer.hpp>

#include <cmath>
#include <cstring>

#include <fstream>
#include <sstream>
#include <tinyxml2.h>

namespace srsp::model {
namespace {

using tinyxml2::XMLElement;

Vec3 parse_vec3(const char* text, Vec3 fallback) {
  if (text == nullptr) {
    return fallback;
  }
  Vec3 value = fallback;
  std::istringstream stream(text);
  stream >> value[0] >> value[1] >> value[2];
  return value;
}

Scalar parse_scalar(const char* text, Scalar fallback) {
  if (text == nullptr) {
    return fallback;
  }
  try {
    return std::stod(text);
  } catch (const std::exception&) {
    return fallback;
  }
}

/// Converts a unit quaternion (w, x, y, z in MJCF) to roll/pitch/yaw.
Vec3 quat_to_rpy(const char* text) {
  Vec3 rpy{};
  if (text == nullptr) {
    return rpy;
  }
  std::istringstream stream(text);
  double w = 1.0;
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  if (!(stream >> w >> x >> y >> z)) {
    return rpy;
  }
  const double norm = std::sqrt(w * w + x * x + y * y + z * z);
  if (norm <= 0.0) {
    return rpy;
  }
  w /= norm;
  x /= norm;
  y /= norm;
  z /= norm;

  // Rotation matrix from the quaternion, then RPY with R = Rz * Ry * Rx.
  const double r00 = 1.0 - 2.0 * (y * y + z * z);
  const double r10 = 2.0 * (x * y + w * z);
  const double r20 = 2.0 * (x * z - w * y);
  const double r21 = 2.0 * (y * z + w * x);
  const double r22 = 1.0 - 2.0 * (x * x + y * y);
  rpy[0] = std::atan2(r21, r22);
  rpy[1] = std::asin(-r20 < -1.0 ? -1.0 : (-r20 > 1.0 ? 1.0 : -r20));
  rpy[2] = std::atan2(r10, r00);
  return rpy;
}

/// Parses "lower upper" ranges; a single value means a symmetric range.
void parse_range(const char* text, Joint* joint) {
  if (text == nullptr) {
    return;
  }
  std::istringstream stream(text);
  double lower = 0.0;
  double upper = 0.0;
  if (stream >> lower >> upper) {
    joint->lower = lower;
    joint->upper = upper;
  }
}

void parse_inertial(const XMLElement* body, Body* out) {
  const XMLElement* inertial = body->FirstChildElement("inertial");
  const XMLElement* source = inertial != nullptr ? inertial : body;
  if (source->Attribute("mass") != nullptr) {
    out->inertia.mass = parse_scalar(source->Attribute("mass"), 0.0);
  }
  if (source->Attribute("pos") != nullptr) {
    out->inertia.com = parse_vec3(source->Attribute("pos"), Vec3{});
  }
  if (source->Attribute("diaginertia") != nullptr) {
    const Vec3 diagonal = parse_vec3(source->Attribute("diaginertia"), Vec3{});
    out->inertia.inertia = {diagonal[0], 0.0, 0.0, 0.0, diagonal[1], 0.0, 0.0, 0.0, diagonal[2]};
  }
}

/// Recursively converts one <body> element and its children.
void convert_body(const XMLElement* element, const std::string& parent, Model* model,
                  std::vector<ValidationIssue>* issues) {
  Body body;
  body.id = element->Attribute("name") != nullptr ? element->Attribute("name") : "";
  if (body.id.empty()) {
    issues->push_back({"<body>", "body without a name; skipped"});
    return;
  }
  parse_inertial(element, &body);

  const XMLElement* joint = element->FirstChildElement("joint");
  const XMLElement* free_joint = element->FirstChildElement("freejoint");

  if (free_joint != nullptr && !parent.empty()) {
    issues->push_back({body.id, "freejoint on a non-root body is not supported"});
  } else if (joint != nullptr && parent.empty()) {
    issues->push_back({body.id, "joint on a root body is not supported; use freejoint"});
  } else if (joint != nullptr) {
    const char* type = joint->Attribute("type");
    Joint converted;
    converted.id =
        joint->Attribute("name") != nullptr ? joint->Attribute("name") : body.id + "_joint";
    converted.parent_body = parent;
    converted.child_body = body.id;
    converted.axis = parse_vec3(joint->Attribute("axis"), Vec3{0.0, 0.0, 1.0});
    converted.origin = parse_vec3(element->Attribute("pos"), Vec3{});
    converted.origin_rpy = quat_to_rpy(element->Attribute("quat"));
    parse_range(joint->Attribute("range"), &converted);

    if (type == nullptr || std::strcmp(type, "hinge") == 0) {
      converted.type = JointType::kRevolute;
      if (type == nullptr) {
        converted.lower = -3.14159265358979323846;
        converted.upper = 3.14159265358979323846;
      }
    } else if (std::strcmp(type, "slide") == 0) {
      converted.type = JointType::kPrismatic;
    } else {
      issues->push_back({converted.id, std::string("unsupported joint type: ") + type});
      converted.type = JointType::kFixed;
    }
    model->joints.push_back(std::move(converted));
    body.parent_joint = model->joints.back().id;
  }

  model->bodies.push_back(std::move(body));
  const std::string current = model->bodies.back().id;

  for (const XMLElement* child = element->FirstChildElement("body"); child != nullptr;
       child = child->NextSiblingElement("body")) {
    convert_body(child, current, model, issues);
  }
}

}  // namespace

LoadResult import_mjcf(const std::string& xml) {
  LoadResult result;
  tinyxml2::XMLDocument document;
  if (document.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) {
    result.issues.push_back({"<mjcf>", "XML parse error"});
    return result;
  }

  const XMLElement* mujoco = document.FirstChildElement("mujoco");
  if (mujoco == nullptr) {
    result.issues.push_back({"<mjcf>", "missing <mujoco> root element"});
    return result;
  }
  const XMLElement* worldbody = mujoco->FirstChildElement("worldbody");
  if (worldbody == nullptr) {
    result.issues.push_back({"<mjcf>", "missing <worldbody>"});
    return result;
  }

  result.model.name =
      mujoco->Attribute("model") != nullptr ? mujoco->Attribute("model") : "mjcf_import";
  result.model.version = "1.0";

  // The MJCF worldbody itself is not a body; its children are the roots.
  for (const XMLElement* child = worldbody->FirstChildElement("body"); child != nullptr;
       child = child->NextSiblingElement("body")) {
    convert_body(child, "", &result.model, &result.issues);
  }

  const auto structural = validate(result.model);
  result.issues.insert(result.issues.end(), structural.begin(), structural.end());
  return result;
}

LoadResult import_mjcf_file(const std::string& path) {
  LoadResult result;
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    result.issues.push_back({path, "cannot open MJCF file"});
    return result;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return import_mjcf(buffer.str());
}

}  // namespace srsp::model
