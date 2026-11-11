/**
 * @file assembly.hpp
 * @brief W2: compile components with mount relations into one canonical SRM model.
 *
 * A mount is a directed edge (child mounted on a host). Compilation validates
 * the graph, then produces a single `srsp::model::Model` whose body and joint
 * ids carry the instance prefix, so the whole assembly is one dynamics tree and
 * CRBA sees every coupling.
 */
#ifndef SRSP_MODEL_ASSEMBLY_HPP
#define SRSP_MODEL_ASSEMBLY_HPP

#include <srsp/model/srm.hpp>

#include <array>
#include <string>
#include <vector>

namespace srsp::model {

/** @brief How a component attaches to its host. */
enum class MountKind : std::int32_t {
  kFixed = 0,    ///< Rigid attachment; mass joins the tree.
  kJointed = 1,  ///< Adds a degree of freedom (gimbal, arm base).
  kLogical = 2,  ///< No spatial contribution (controller, software).
};

/** @brief Declares that a component is mounted on a host component. */
struct MountSpec {
  std::string component_id;  ///< Child instance id, e.g. "entity/camera".
  std::string host;          ///< Host instance id; empty means the base.
  std::string frame;         ///< Host frame name; "body" and "" are always valid.
  Vec3 position_m{};         ///< Offset in the host frame, metres.
  std::array<double, 4> orientation{0.0, 0.0, 0.0, 1.0};  ///< x, y, z, w.
  MountKind kind = MountKind::kFixed;
  double mass_kg = 0.0;  ///< 0 keeps the host's nv unchanged.
};

/** @brief One component instance's contribution to the assembly. */
struct ComponentFragment {
  std::string id;                   ///< Instance id, unique in the assembly.
  double mass_kg = 0.0;             ///< Total mass contributed when mounted fixed.
  Vec3 com_m{};                     ///< Centre of mass in the fragment frame.
  std::array<double, 9> inertia{};  ///< Row-major tensor about the com.
  std::vector<Body> bodies;         ///< Bodies declared by this component.
  std::vector<Joint> joints;        ///< Internal joints.
  std::vector<Actuator> actuators;
  std::vector<Sensor> sensors;
  std::vector<MountSpec> mounts;  ///< Children mounted on this component.
};

/** @brief Assembly to compile. */
struct AssemblySpec {
  std::string id;
  ComponentFragment base;                ///< The root component.
  std::vector<ComponentFragment> parts;  ///< Everything else.
};

/** @brief Compilation outcome. */
struct AssemblyResult {
  bool ok = false;
  Model model;
  std::vector<std::string> errors;
  double total_mass_kg = 0.0;
  int dynamic_bodies = 0;
};

/**
 * @brief Compiles components and mounts into one canonical model.
 *
 * Validation: unique instance ids, every host exists, no cycles, frames
 * resolvable, non-negative mass. A fixed mount with mass > 0 becomes a child
 * body joined by a fixed joint; a zero-mass or logical mount only records a
 * sensor frame and leaves the joint count unchanged.
 *
 * @param spec Assembly description.
 * @return Result with the merged model, errors and summed mass.
 */
[[nodiscard]] AssemblyResult build_assembly(const AssemblySpec& spec);

/** @brief Canonical instance prefix, e.g. "entity/satellite" -> "satellite/". */
[[nodiscard]] std::string instance_prefix(const std::string& instance_id);

}  // namespace srsp::model

#endif  // SRSP_MODEL_ASSEMBLY_HPP
