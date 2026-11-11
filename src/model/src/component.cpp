/**
 * @file component.cpp
 * @brief Implementation of component manifest validation and identity rules.
 */
#include <srsp/model/component.hpp>

#include <algorithm>
#include <array>

#include <cctype>
#include <regex>
#include <sstream>

namespace srsp::model {

namespace {

constexpr std::array<std::pair<std::string_view, ComponentKind>, 11> kKinds{{
    {"env.celestial", ComponentKind::kCelestial},
    {"dynamics.body", ComponentKind::kBody},
    {"dynamics.joint", ComponentKind::kJoint},
    {"dynamics.force", ComponentKind::kForce},
    {"dynamics.actuator", ComponentKind::kActuator},
    {"dynamics.propulsion", ComponentKind::kPropulsion},
    {"sensor.camera", ComponentKind::kSensor},
    {"sensor.lidar", ComponentKind::kSensor},
    {"sensor.radar", ComponentKind::kSensor},
    {"control.controller", ComponentKind::kController},
    {"visual.renderer", ComponentKind::kRenderer},
}};

// The manifest spells sensor kinds with a subtype, so the matrix above maps all
// three to kSensor while validation still checks the exact spelling.
constexpr std::array<std::string_view, 11> kKindNames{{
    "env.celestial",
    "dynamics.body",
    "dynamics.joint",
    "dynamics.force",
    "dynamics.actuator",
    "dynamics.propulsion",
    "sensor.camera",
    "sensor.lidar",
    "sensor.radar",
    "control.controller",
    "visual.renderer",
}};

constexpr std::array<std::string_view, 4> kVisualOnlyKeys{{
    "texture",
    "material",
    "mesh_uri",
    "albedo",
}};

void issue(std::vector<ComponentIssue>* issues, std::string path, std::string message) {
  if (issues != nullptr) {
    issues->push_back(ComponentIssue{std::move(path), std::move(message)});
  }
}

bool is_semver(const std::string& text) {
  static const std::regex pattern(R"(^[0-9]+\.[0-9]+\.[0-9]+$)");
  return std::regex_match(text, pattern);
}

bool is_component_id(const std::string& text) {
  static const std::regex pattern(R"(^[a-z][a-z0-9_-]*(\.[a-z0-9_-]+)+$)");
  return std::regex_match(text, pattern);
}

bool is_capability_id(const std::string& text) {
  static const std::regex pattern(R"(^srsp\.[a-z]+/[a-z0-9_]+@[0-9]+$)");
  return std::regex_match(text, pattern);
}

bool satisfies_type(const nlohmann::json& value, const std::string& type) {
  if (type == "number") {
    return value.is_number();
  }
  if (type == "integer") {
    return value.is_number_integer() || value.is_number_unsigned();
  }
  if (type == "boolean") {
    return value.is_boolean();
  }
  if (type == "string") {
    return value.is_string();
  }
  if (type == "object") {
    return value.is_object();
  }
  if (type == "array") {
    return value.is_array();
  }
  return false;
}

}  // namespace

std::optional<ComponentKind> parse_component_kind(std::string_view text) {
  for (std::size_t i = 0; i < kKindNames.size(); ++i) {
    if (kKindNames[i] == text) {
      return kKinds[i].second;
    }
  }
  return std::nullopt;
}

const char* to_string(ComponentKind kind) noexcept {
  switch (kind) {
    case ComponentKind::kCelestial:
      return "env.celestial";
    case ComponentKind::kBody:
      return "dynamics.body";
    case ComponentKind::kJoint:
      return "dynamics.joint";
    case ComponentKind::kForce:
      return "dynamics.force";
    case ComponentKind::kActuator:
      return "dynamics.actuator";
    case ComponentKind::kPropulsion:
      return "dynamics.propulsion";
    case ComponentKind::kSensor:
      return "sensor.camera";
    case ComponentKind::kController:
      return "control.controller";
    case ComponentKind::kRenderer:
      return "visual.renderer";
  }
  return "unknown";
}

bool contributes_to_dynamics(ComponentKind kind) noexcept {
  switch (kind) {
    case ComponentKind::kBody:
    case ComponentKind::kJoint:
    case ComponentKind::kActuator:
    case ComponentKind::kPropulsion:
      return true;
    default:
      return false;
  }
}

bool validate_component_manifest(const nlohmann::json& document, ComponentManifest* manifest,
                                 std::vector<ComponentIssue>* issues) {
  const std::size_t before = issues != nullptr ? issues->size() : 0u;

  if (!document.is_object()) {
    issue(issues, "<document>", "manifest must be a JSON object");
    return false;
  }
  if (!document.contains("schema") || !document["schema"].is_string() ||
      document["schema"] != "srsp.component/1") {
    issue(issues, "schema", "expected \"srsp.component/1\"");
  }

  ComponentKind kind = ComponentKind::kBody;
  const bool has_kind = document.contains("kind") && document["kind"].is_string();
  if (!has_kind) {
    issue(issues, "kind", "missing required field");
  } else if (const auto parsed = parse_component_kind(document["kind"].get<std::string>())) {
    kind = *parsed;
  } else {
    issue(issues, "kind", "unknown component kind: " + document["kind"].get<std::string>());
  }

  std::string id;
  if (!document.contains("id") || !document["id"].is_string()) {
    issue(issues, "id", "missing required field");
  } else {
    id = document["id"].get<std::string>();
    if (!is_component_id(id)) {
      issue(issues, "id", "expected dotted lowercase id, e.g. org.srsp.sensors.camera");
    }
  }

  std::string version = "0.0.0";
  if (!document.contains("version") || !document["version"].is_string()) {
    issue(issues, "version", "missing required field");
  } else {
    version = document["version"].get<std::string>();
    if (!is_semver(version)) {
      issue(issues, "version", "expected MAJOR.MINOR.PATCH");
    }
  }

  std::vector<std::string> capabilities;
  if (!document.contains("capabilities") || !document["capabilities"].is_array() ||
      document["capabilities"].empty()) {
    issue(issues, "capabilities", "expected a non-empty array");
  } else {
    for (std::size_t i = 0; i < document["capabilities"].size(); ++i) {
      const auto& entry = document["capabilities"][i];
      const std::string path = "capabilities[" + std::to_string(i) + "]";
      if (!entry.is_string()) {
        issue(issues, path, "expected a string");
        continue;
      }
      const std::string value = entry.get<std::string>();
      if (!is_capability_id(value)) {
        issue(issues, path, "expected capability@major, e.g. srsp.sensor/device@1");
      }
      capabilities.push_back(value);
    }
  }

  // Parameter schema: every required key must be declared, and every declared
  // property must carry a type so defaults can be checked.
  if (document.contains("params")) {
    const auto& params = document["params"];
    if (!params.is_object()) {
      issue(issues, "params", "expected an object");
    } else {
      if (params.contains("type") && params["type"] != "object") {
        issue(issues, "params.type", "only \"object\" is supported");
      }
      const auto& properties =
          params.contains("properties") ? params["properties"] : nlohmann::json::object();
      if (!properties.is_object()) {
        issue(issues, "params.properties", "expected an object");
      }
      for (const auto& [name, spec] : properties.items()) {
        const std::string path = "params.properties." + name;
        if (std::find(kVisualOnlyKeys.begin(), kVisualOnlyKeys.end(), name) !=
            kVisualOnlyKeys.end()) {
          issue(issues, path, "visual resources belong in the top-level visual object");
        }
        if (!spec.is_object() || !spec.contains("type") || !spec["type"].is_string()) {
          issue(issues, path, "declared property needs a type");
          continue;
        }
        if (spec.contains("default") && !satisfies_type(spec["default"], spec["type"])) {
          issue(issues, path + ".default", "default does not match the declared type");
        }
      }
      if (params.contains("required")) {
        if (!params["required"].is_array()) {
          issue(issues, "params.required", "expected an array");
        } else {
          for (std::size_t i = 0; i < params["required"].size(); ++i) {
            const auto& entry = params["required"][i];
            const std::string path = "params.required[" + std::to_string(i) + "]";
            if (!entry.is_string()) {
              issue(issues, path, "expected a property name");
              continue;
            }
            if (!properties.contains(entry.get<std::string>())) {
              issue(issues, path, "required property is not declared in properties");
            }
          }
        }
      }
    }
  }

  if (document.contains("mount")) {
    const auto& mount = document["mount"];
    if (!mount.is_object()) {
      issue(issues, "mount", "expected an object");
    } else {
      if (mount.contains("kind")) {
        const std::string mount_kind =
            mount["kind"].is_string() ? mount["kind"].get<std::string>() : std::string();
        if (mount_kind != "fixed" && mount_kind != "jointed" && mount_kind != "logical") {
          issue(issues, "mount.kind", "expected fixed, jointed or logical");
        }
      }
      if (mount.contains("default_mass_kg")) {
        if (!mount["default_mass_kg"].is_number()) {
          issue(issues, "mount.default_mass_kg", "expected a number");
        } else if (mount["default_mass_kg"].get<double>() < 0.0) {
          issue(issues, "mount.default_mass_kg", "mass must not be negative");
        }
      }
    }
  }

  if (document.contains("visual") && !document["visual"].is_object()) {
    issue(issues, "visual", "expected an object");
  }
  if (document.contains("checkpoint") && !document["checkpoint"].is_boolean()) {
    issue(issues, "checkpoint", "expected a boolean");
  }

  const bool ok = issues == nullptr || issues->size() == before;
  if (ok && manifest != nullptr) {
    manifest->kind = kind;
    manifest->id = std::move(id);
    manifest->version = std::move(version);
    manifest->capabilities = std::move(capabilities);
    manifest->checkpoint = document.contains("checkpoint") && document["checkpoint"].is_boolean() &&
                           document["checkpoint"].get<bool>();
  }
  return ok;
}

std::string format_issues(const std::vector<ComponentIssue>& issues) {
  std::ostringstream stream;
  for (std::size_t i = 0; i < issues.size(); ++i) {
    if (i != 0) {
      stream << '\n';
    }
    stream << issues[i].path << ": " << issues[i].message;
  }
  return stream.str();
}

void ParamResolver::set_default(std::string name, std::string value) {
  values_[std::move(name)] = ResolvedParam{std::move(value), ParamSource::kDefault};
}

void ParamResolver::set_model(std::string name, std::string value) {
  values_[std::move(name)] = ResolvedParam{std::move(value), ParamSource::kModel};
}

void ParamResolver::set_project(std::string name, std::string value) {
  values_[std::move(name)] = ResolvedParam{std::move(value), ParamSource::kProject};
}

void ParamResolver::set_cli(std::string name, std::string value) {
  values_[std::move(name)] = ResolvedParam{std::move(value), ParamSource::kCli};
}

std::optional<ResolvedParam> ParamResolver::resolve(std::string_view name) const {
  const auto it = values_.find(std::string(name));
  if (it == values_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::map<std::string, ResolvedParam> ParamResolver::resolved() const {
  std::map<std::string, ResolvedParam> copy;
  for (const auto& [name, value] : values_) {
    if (value.source == ParamSource::kCli || value.source == ParamSource::kProject ||
        value.source == ParamSource::kModel || value.source == ParamSource::kDefault) {
      copy.emplace(name, value);
    }
  }
  return copy;
}

bool EntityCatalog::add(std::string id, std::string source, std::string* error) {
  const auto it = sources_.find(id);
  if (it != sources_.end()) {
    if (error != nullptr) {
      *error =
          "duplicate entity id '" + id + "' declared by '" + it->second + "' and '" + source + "'";
    }
    return false;
  }
  sources_.emplace(std::move(id), std::move(source));
  return true;
}

bool EntityCatalog::contains(std::string_view id) const {
  return sources_.find(std::string(id)) != sources_.end();
}

}  // namespace srsp::model
