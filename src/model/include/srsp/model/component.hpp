/**
 * @file component.hpp
 * @brief Component manifest validation, entity identity and parameter resolution.
 *
 * W1 of the component plan: a component is not "a class" but a manifest plus a
 * typed parameter schema, an instance identity and a resolution order. This
 * header keeps those rules in one place so loaders, tests and the CLI agree.
 */
#ifndef SRSP_MODEL_COMPONENT_HPP
#define SRSP_MODEL_COMPONENT_HPP

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace srsp::model {

/** @brief Component family; drives which parameters and contributions apply. */
enum class ComponentKind : std::int32_t {
  kCelestial = 0,   ///< Environment body: mu, radius, ephemeris.
  kBody = 1,        ///< Rigid body with mass properties.
  kJoint = 2,       ///< Mechanism degree of freedom.
  kForce = 3,       ///< Force/torque model.
  kActuator = 4,    ///< Wheel or thruster.
  kPropulsion = 5,  ///< Propellant tank and mass flow.
  kSensor = 6,      ///< Camera, lidar or radar.
  kController = 7,  ///< Control law.
  kRenderer = 8,    ///< Visual backend.
};

/** @brief Parses the manifest spelling of a kind. */
/** @param text One of "env.celestial", "dynamics.body", ... */
/** @return The kind, or empty when unknown. */
[[nodiscard]] std::optional<ComponentKind> parse_component_kind(std::string_view text);

/** @brief Canonical manifest spelling of a kind. */
[[nodiscard]] const char* to_string(ComponentKind kind) noexcept;

/** @brief True when the kind adds mass or a degree of freedom to dynamics. */
[[nodiscard]] bool contributes_to_dynamics(ComponentKind kind) noexcept;

/** @brief One validation problem, located by JSON path. */
struct ComponentIssue {
  std::string path;     ///< e.g. "params.required[0]".
  std::string message;  ///< Human-readable reason.
};

/** @brief Parsed manifest, valid after validate_component_manifest(). */
struct ComponentManifest {
  ComponentKind kind = ComponentKind::kBody;
  std::string id;
  std::string version;
  std::vector<std::string> capabilities;
  bool checkpoint = false;
};

/**
 * @brief Validates a component.json document.
 * @param document Parsed JSON.
 * @param manifest Optional output for the parsed fields.
 * @param issues Receives every problem found (not just the first).
 * @return True when no issue was recorded.
 */
[[nodiscard]] bool validate_component_manifest(const nlohmann::json& document,
                                               ComponentManifest* manifest,
                                               std::vector<ComponentIssue>* issues);

/** @brief Error text for a set of issues, one line per problem. */
[[nodiscard]] std::string format_issues(const std::vector<ComponentIssue>& issues);

/** @brief Where a resolved parameter value came from, highest priority first. */
enum class ParamSource : std::int32_t {
  kDefault = 0,
  kModel = 1,
  kProject = 2,
  kCli = 3,
};

/** @brief A resolved value and its provenance. */
struct ResolvedParam {
  std::string value;
  ParamSource source = ParamSource::kDefault;
};

/**
 * @brief Four-layer parameter resolution: CLI > project > model > default.
 *
 * Components must not read CLI flags or environment variables themselves; they
 * ask this resolver once and receive a single value with its provenance.
 */
class ParamResolver {
 public:
  /** @brief Records a default (lowest priority). */
  void set_default(std::string name, std::string value);
  /** @brief Records a value declared by the model. */
  void set_model(std::string name, std::string value);
  /** @brief Records a project-level override. */
  void set_project(std::string name, std::string value);
  /** @brief Records a CLI override (highest priority). */
  void set_cli(std::string name, std::string value);

  /** @brief Resolves a parameter, or empty when no layer defined it. */
  [[nodiscard]] std::optional<ResolvedParam> resolve(std::string_view name) const;

  /** @brief All parameters visible after resolution, sorted by name. */
  [[nodiscard]] std::map<std::string, ResolvedParam> resolved() const;

 private:
  std::map<std::string, ResolvedParam> values_;
};

/**
 * @brief Enforces globally unique entity ids.
 *
 * Two sources claiming the same id is a load error, not a last-writer-wins
 * overwrite; the diagnostic names both sources.
 */
class EntityCatalog {
 public:
  /**
   * @brief Registers an entity id.
   * @param id Namespaced id, e.g. "entity/satellite".
   * @param source Where the declaration came from, for diagnostics.
   * @param error Receives the conflict description on failure.
   * @return True when the id was free.
   */
  bool add(std::string id, std::string source, std::string* error);

  /** @brief True when @p id is already registered. */
  [[nodiscard]] bool contains(std::string_view id) const;

  /** @brief Number of registered entities. */
  [[nodiscard]] std::size_t size() const noexcept { return sources_.size(); }

 private:
  std::map<std::string, std::string> sources_;
};

}  // namespace srsp::model

#endif  // SRSP_MODEL_COMPONENT_HPP
