/**
 * @file urdf_importer.cpp
 * @brief URDF parsing (tinyxml2) and conversion into SRM.
 */
#include <srsp/model/urdf_importer.hpp>

#include <cstring>

#include <fstream>
#include <sstream>
#include <tinyxml2.h>
#include <unordered_set>

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

JointType map_joint_type(const char* type, bool* supported) {
  *supported = true;
  if (type == nullptr) {
    *supported = false;
    return JointType::kFixed;
  }
  if (std::strcmp(type, "revolute") == 0) {
    return JointType::kRevolute;
  }
  if (std::strcmp(type, "continuous") == 0) {
    return JointType::kContinuous;
  }
  if (std::strcmp(type, "prismatic") == 0) {
    return JointType::kPrismatic;
  }
  if (std::strcmp(type, "fixed") == 0) {
    return JointType::kFixed;
  }
  *supported = false;
  return JointType::kFixed;
}

void parse_inertial(const XMLElement* link, Body* body, std::vector<ValidationIssue>* issues) {
  const XMLElement* inertial = link->FirstChildElement("inertial");
  if (inertial == nullptr) {
    issues->push_back({body->id, "link has no inertial block; treating as massless"});
    return;
  }
  if (const XMLElement* mass = inertial->FirstChildElement("mass")) {
    body->inertia.mass = parse_scalar(mass->Attribute("value"), 0.0);
  }
  if (const XMLElement* origin = inertial->FirstChildElement("origin")) {
    body->inertia.com = parse_vec3(origin->Attribute("xyz"), Vec3{});
  }
  if (const XMLElement* inertia = inertial->FirstChildElement("inertia")) {
    Mat3& tensor = body->inertia.inertia;
    tensor[0] = parse_scalar(inertia->Attribute("ixx"), 0.0);
    tensor[4] = parse_scalar(inertia->Attribute("iyy"), 0.0);
    tensor[8] = parse_scalar(inertia->Attribute("izz"), 0.0);
    tensor[1] = tensor[3] = parse_scalar(inertia->Attribute("ixy"), 0.0);
    tensor[2] = tensor[6] = parse_scalar(inertia->Attribute("ixz"), 0.0);
    tensor[5] = tensor[7] = parse_scalar(inertia->Attribute("iyz"), 0.0);
  }
}

void parse_visuals(const XMLElement* link, Body* body) {
  for (const XMLElement* visual = link->FirstChildElement("visual"); visual != nullptr;
       visual = visual->NextSiblingElement("visual")) {
    const XMLElement* geometry = visual->FirstChildElement("geometry");
    if (geometry == nullptr) {
      continue;
    }
    const XMLElement* mesh = geometry->FirstChildElement("mesh");
    if (mesh == nullptr) {
      continue;
    }
    Geometry item;
    item.mesh = mesh->Attribute("filename") != nullptr ? mesh->Attribute("filename") : "";
    item.scale = parse_vec3(mesh->Attribute("scale"), Vec3{1.0, 1.0, 1.0});
    item.collision = false;
    body->geometry.push_back(std::move(item));
  }
}

}  // namespace

LoadResult import_urdf(const std::string& xml) {
  LoadResult result;
  tinyxml2::XMLDocument document;
  if (document.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) {
    result.issues.push_back({"<urdf>", "XML parse error"});
    return result;
  }

  const XMLElement* robot = document.FirstChildElement("robot");
  if (robot == nullptr) {
    result.issues.push_back({"<urdf>", "missing <robot> root element"});
    return result;
  }

  Model& model = result.model;
  model.name = robot->Attribute("name") != nullptr ? robot->Attribute("name") : "urdf_import";
  model.version = "1.0";

  for (const XMLElement* link = robot->FirstChildElement("link"); link != nullptr;
       link = link->NextSiblingElement("link")) {
    Body body;
    body.id = link->Attribute("name") != nullptr ? link->Attribute("name") : "";
    if (body.id.empty()) {
      result.issues.push_back({"<link>", "link without a name"});
      continue;
    }
    parse_inertial(link, &body, &result.issues);
    parse_visuals(link, &body);
    model.bodies.push_back(std::move(body));
  }

  std::unordered_set<std::string> child_links;
  for (const XMLElement* joint = robot->FirstChildElement("joint"); joint != nullptr;
       joint = joint->NextSiblingElement("joint")) {
    Joint item;
    item.id = joint->Attribute("name") != nullptr ? joint->Attribute("name") : "";
    const XMLElement* parent = joint->FirstChildElement("parent");
    const XMLElement* child = joint->FirstChildElement("child");
    if (item.id.empty() || parent == nullptr || child == nullptr) {
      result.issues.push_back(
          {item.id.empty() ? "<joint>" : item.id, "joint needs a name, a parent and a child"});
      continue;
    }
    item.parent_body = parent->Attribute("link") != nullptr ? parent->Attribute("link") : "";
    item.child_body = child->Attribute("link") != nullptr ? child->Attribute("link") : "";
    child_links.insert(item.child_body);

    bool supported = false;
    item.type = map_joint_type(joint->Attribute("type"), &supported);
    if (!supported) {
      result.issues.push_back(
          {item.id, std::string("unsupported joint type: ") + joint->Attribute("type")});
      continue;
    }
    if (const XMLElement* origin = joint->FirstChildElement("origin")) {
      item.origin = parse_vec3(origin->Attribute("xyz"), Vec3{});
      item.origin_rpy = parse_vec3(origin->Attribute("rpy"), Vec3{});
    }
    if (const XMLElement* axis = joint->FirstChildElement("axis")) {
      item.axis = parse_vec3(axis->Attribute("xyz"), Vec3{0.0, 0.0, 1.0});
    }
    if (const XMLElement* limit = joint->FirstChildElement("limit")) {
      item.lower = parse_scalar(limit->Attribute("lower"), 0.0);
      item.upper = parse_scalar(limit->Attribute("upper"), 0.0);
    }
    model.joints.push_back(std::move(item));
  }

  // Attach joints to their child bodies so the SRM tree matches the URDF one.
  for (const auto& joint : model.joints) {
    for (auto& body : model.bodies) {
      if (body.id == joint.child_body) {
        body.parent_joint = joint.id;
      }
    }
  }
  for (auto& body : model.bodies) {
    if (child_links.find(body.id) == child_links.end()) {
      body.parent_joint.clear();
    }
  }

  const auto structural = validate(model);
  result.issues.insert(result.issues.end(), structural.begin(), structural.end());
  return result;
}

LoadResult import_urdf_file(const std::string& path) {
  LoadResult result;
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    result.issues.push_back({path, "cannot open URDF file"});
    return result;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return import_urdf(buffer.str());
}

}  // namespace srsp::model
