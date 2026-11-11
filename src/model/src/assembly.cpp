/**
 * @file assembly.cpp
 * @brief Implementation of mount-graph validation and model merging (W2).
 */
#include <srsp/model/assembly.hpp>

#include <algorithm>
#include <functional>
#include <map>
#include <string_view>

#include <set>
#include <sstream>

namespace srsp::model {

namespace {

bool valid_frame(std::string_view frame) {
  // "body" and "" always exist; anything else must be a declared sensor frame,
  // which the fragment carries in its sensor list.
  return frame.empty() || frame == "body";
}

void add_error(AssemblyResult* result, std::string text) {
  result->errors.push_back(std::move(text));
}

}  // namespace

std::string instance_prefix(const std::string& instance_id) {
  const std::size_t slash = instance_id.find_last_of('/');
  const std::string name = slash == std::string::npos ? instance_id : instance_id.substr(slash + 1);
  return name + "/";
}

AssemblyResult build_assembly(const AssemblySpec& spec) {
  AssemblyResult result;

  // 1. Unique instance ids.
  std::map<std::string, std::string> seen;
  std::vector<const ComponentFragment*> fragments;
  fragments.push_back(&spec.base);
  for (const auto& part : spec.parts) {
    fragments.push_back(&part);
  }
  for (const auto* fragment : fragments) {
    if (fragment->id.empty()) {
      add_error(&result, "component with empty instance id");
      continue;
    }
    const auto [it, inserted] = seen.emplace(fragment->id, fragment->id);
    (void)it;
    if (!inserted) {
      add_error(&result, "duplicate component instance id '" + fragment->id + "'");
    }
  }

  // 2. Mount edges: host exists, frame resolvable, no cycles.
  std::map<std::string, std::vector<std::string>> children;  // host -> children
  std::map<std::string, MountSpec> edge;                     // child -> mount
  for (const auto* fragment : fragments) {
    for (const auto& mount : fragment->mounts) {
      if (mount.component_id.empty()) {
        add_error(&result, "mount without a component id on '" + fragment->id + "'");
        continue;
      }
      if (seen.find(mount.component_id) == seen.end()) {
        add_error(&result, "mount target '" + mount.component_id + "' is not a declared component");
        continue;
      }
      if (seen.find(mount.host) == seen.end()) {
        add_error(&result, "mount host '" + mount.host + "' of '" + mount.component_id +
                               "' is not a declared component");
        continue;
      }
      if (!valid_frame(mount.frame)) {
        add_error(&result, "unknown frame '" + mount.frame + "' on '" + mount.host + "'");
        continue;
      }
      if (mount.mass_kg < 0.0) {
        add_error(&result, "negative mount mass on '" + mount.component_id + "'");
        continue;
      }
      children[mount.host].push_back(mount.component_id);
      edge[mount.component_id] = mount;
    }
  }

  // Depth-first cycle detection over the mount graph.
  std::set<std::string> visiting;
  std::set<std::string> done;
  std::function<bool(const std::string&)> visit = [&](const std::string& node) {
    if (done.count(node) != 0) {
      return false;
    }
    if (visiting.count(node) != 0) {
      return true;
    }
    visiting.insert(node);
    for (const auto& child : children[node]) {
      if (visit(child)) {
        return true;
      }
    }
    visiting.erase(node);
    done.insert(node);
    return false;
  };
  for (const auto* fragment : fragments) {
    if (visit(fragment->id)) {
      add_error(&result, "mount graph contains a cycle at '" + fragment->id + "'");
      break;
    }
  }

  if (!result.errors.empty()) {
    return result;
  }

  // 3. Merge in a stable order: the base first, then parts in declaration order.
  Model model;
  model.name = spec.id;
  auto merge_fragment = [&](const ComponentFragment& fragment, const std::string& parent_body) {
    const std::string prefix = instance_prefix(fragment.id);
    std::string body_id = prefix + "base";
    if (!parent_body.empty()) {
      body_id = prefix + "body";
    }

    Body body;
    body.id = body_id;
    body.inertia.mass = fragment.mass_kg;
    body.inertia.com = fragment.com_m;
    body.inertia.inertia = fragment.inertia;
    model.bodies.push_back(body);

    if (!parent_body.empty()) {
      Joint joint;
      joint.id = prefix + "mount";
      joint.parent_body = parent_body;
      joint.child_body = body_id;
      joint.type =
          edge[fragment.id].kind == MountKind::kJointed ? JointType::kRevolute : JointType::kFixed;
      joint.origin = edge[fragment.id].position_m;
      model.joints.push_back(joint);
    }

    for (const auto& decl : fragment.bodies) {
      Body copy = decl;
      copy.id = prefix + decl.id;
      model.bodies.push_back(copy);
    }
    for (const auto& decl : fragment.joints) {
      Joint copy = decl;
      copy.id = prefix + decl.id;
      copy.parent_body = prefix + decl.parent_body;
      copy.child_body = prefix + decl.child_body;
      model.joints.push_back(copy);
    }
    for (const auto& decl : fragment.actuators) {
      Actuator copy = decl;
      copy.id = prefix + decl.id;
      if (!copy.joint.empty()) {
        copy.joint = prefix + copy.joint;
      }
      if (!copy.body.empty()) {
        copy.body = prefix + copy.body;
      }
      model.actuators.push_back(copy);
    }
    for (const auto& decl : fragment.sensors) {
      Sensor copy = decl;
      copy.id = prefix + decl.id;
      copy.body = prefix + decl.body;
      model.sensors.push_back(copy);
    }
    result.total_mass_kg += fragment.mass_kg;
    ++result.dynamic_bodies;
    return body_id;
  };

  std::map<std::string, std::string> root_body_of;
  root_body_of[spec.base.id] = merge_fragment(spec.base, "");
  for (const auto& part : spec.parts) {
    const auto mount_it = edge.find(part.id);
    if (mount_it == edge.end()) {
      add_error(&result, "component '" + part.id + "' has no mount declaration");
      continue;
    }
    const auto host_it = root_body_of.find(mount_it->second.host);
    if (host_it == root_body_of.end()) {
      add_error(&result,
                "host '" + mount_it->second.host + "' was not merged before '" + part.id + "'");
      continue;
    }
    // A logical or zero-mass mount adds no dynamics body; only its sensors/frames
    // would be recorded (sensor frames are resolved in a later step).
    if (mount_it->second.kind == MountKind::kLogical || mount_it->second.mass_kg <= 0.0) {
      continue;
    }
    root_body_of[part.id] = merge_fragment(part, host_it->second);
  }

  if (!result.errors.empty()) {
    return result;
  }

  result.ok = true;
  result.model = std::move(model);
  return result;
}

}  // namespace srsp::model
