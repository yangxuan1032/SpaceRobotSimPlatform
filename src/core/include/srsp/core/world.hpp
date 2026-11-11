/**
 * @file world.hpp
 * @brief W3: multi-entity world with atomic per-step commits.
 *
 * The world owns the states of several entities, accepts commands only through
 * a queue, and always publishes one canonical snapshot per successful step. A
 * failed step changes nothing: no partial state, no new generation.
 */
#ifndef SRSP_CORE_WORLD_HPP
#define SRSP_CORE_WORLD_HPP

#include <srsp/core/environment.hpp>
#include <srsp/core/snapshot.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace srsp::core {

/** @brief One entity's kinematic state; sorted by id inside a snapshot. */
struct WorldEntity {
  std::string id;                                ///< Unique, e.g. "entity/satellite".
  std::array<double, 3> position_m{};            ///< Inertial position, metres.
  std::array<double, 3> velocity_m_s{};          ///< Inertial velocity, m/s.
  double mass_kg = 1.0;                          ///< Current mass, kg (shrinks as fuel burns).
  double propellant_kg = 0.0;                    ///< Remaining propellant, kg.
  double thrust_n = 0.0;                         ///< Current thrust along +X, N.
  double isp_s = 0.0;                            ///< Specific impulse, s (0 disables thrust).
  double area_m2 = 0.0;                          ///< Cross-sectional area for SRP and drag.
  double reflectivity = 1.0;                     ///< SRP reflectivity coefficient Cr.
  double drag_coefficient = 2.2;                 ///< Drag coefficient Cd.
  std::array<double, 3> center_of_pressure_m{};  ///< Offset from the CoM.
  std::array<double, 3> last_torque_nm{};        ///< Torque diagnostics.
  std::array<double, 4> attitude{0.0, 0.0, 0.0, 1.0};  ///< body -> inertial.
  std::array<double, 3> omega_body_rad_s{};            ///< Body angular rate.
  std::array<double, 9> inertia{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
  double wheel_momentum_nms = 0.0;  ///< Wheel angular momentum about body X.
  double wheel_max_momentum_nms = 0.0;
  double wheel_torque_nm =
      0.0;  ///< Wheel motor torque about body X.  ///< Saturation limit (0 disables).
  double dry_mass_kg = 0.0;           ///< Dry mass; 0 keeps mass_kg constant.
  std::array<double, 3> dry_com_m{};  ///< Dry centre of mass, body frame.
  std::array<double, 9> dry_inertia{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
  std::array<double, 3> tank_com_m{};        ///< Propellant tank centre, body frame.
  std::array<double, 3> center_of_mass_m{};  ///< Recomputed each step.
  double desaturation_torque_nm = 0.0;       ///< Thruster couple used to dump wheel momentum.
  double aero_cp_arm_m = 0.0;  ///< Half-chord arm; CP offset grows with angle of attack.
};

/** @brief A command applied at the start of the next step. */
enum class WorldCommandKind : std::int32_t {
  kDeltaVelocity = 0,   ///< Adds to the target velocity (impulse style).
  kSetWheelTorque = 2,  ///< Sets the wheel motor torque about body X.
  kSetThrust = 1,       ///< Sets the target thrust in newtons along +X.
};

struct WorldCommand {
  std::string entity;                          ///< Target entity id.
  std::array<double, 3> delta_velocity_m_s{};  ///< Added to the target velocity.
  WorldCommandKind kind = WorldCommandKind::kDeltaVelocity;
  double value = 0.0;  ///< Thrust in newtons for kSetThrust.
};

/**
 * @brief Owns entity states, applies commands and publishes one snapshot per step.
 *
 * Ordering is deterministic: entities stay sorted by id and commands apply in
 * push order. Encoding is native-endian: for each entity a 64-byte id followed
 * by three doubles of position and three of velocity.
 */
class World {
 public:
  /** @brief Creates a world retaining @p retained_snapshots recent snapshots. */
  explicit World(std::size_t retained_snapshots = 4);

  /**
   * @brief Registers an entity.
   * @param entity Initial state; the id must be unique.
   * @param error Receives the conflict description on failure.
   * @return True when the entity was added.
   */
  bool add_entity(WorldEntity entity, std::string* error);

  /** @brief Queues a command for the next step. */
  void push_command(WorldCommand command);

  /**
   * @brief Advances every entity by @p dt seconds and publishes a snapshot.
   * @param dt Step length in seconds; must be finite and non-negative.
   * @return False with @p error set when nothing was committed.
   */
  bool step(double dt, std::string* error);

  /** @brief Entity states in canonical (id-sorted) order. */
  [[nodiscard]] const std::vector<WorldEntity>& entities() const noexcept { return entities_; }

  /** @brief Finds an entity, or nullptr. */
  [[nodiscard]] const WorldEntity* find(std::string_view id) const;

  /** @brief Number of committed steps. */
  [[nodiscard]] std::uint64_t step_index() const noexcept { return step_index_; }

  /** @brief Id of the newest published snapshot. */
  [[nodiscard]] SnapshotId latest_id() const { return store_.latest_id(); }

  /** @brief Lease on a specific snapshot, or empty when unknown/evicted. */
  [[nodiscard]] std::optional<SnapshotStore::Lease> acquire(SnapshotId id) const {
    return store_.acquire(id);
  }

  /** @brief Lease on the newest snapshot, or empty before the first step. */
  [[nodiscard]] std::optional<SnapshotStore::Lease> acquire_latest() const {
    return store_.acquire_latest();
  }

  /**
   * @brief Enables gravitational integration from the environment sample.
   *
   * Without an environment the world integrates constant velocity (legacy
   * path); with one it evaluates gravity at each RK4 stage.
   */
  void set_environment(const EnvironmentSpec& spec);

  /** @brief True when gravity is integrated from the environment. */
  [[nodiscard]] bool has_environment() const noexcept { return environment_.has_value(); }

  /** @brief Called after every committed step, before the next step begins. */
  std::function<void(const SnapshotId&, const std::vector<WorldEntity>&)> on_commit;

  /** @brief Bytes per entity in the encoded snapshot. */
  static constexpr std::size_t kBytesPerEntity = 64 + 14 * sizeof(double);

  /** @brief Snapshot header: magic, version, stride, entity count (16 bytes). */
  static constexpr std::size_t kSnapshotHeaderBytes = 16;
  /** @brief Current snapshot layout version. */
  static constexpr std::uint32_t kSnapshotVersion = 2;

  /** @brief Decoded snapshot header. */
  struct SnapshotHeader {
    std::uint32_t version = 0;
    std::uint32_t stride = 0;
    std::uint32_t count = 0;
  };

  /** @brief Reads the header of an encoded snapshot (all zeroes when invalid). */
  [[nodiscard]] static SnapshotHeader decode_header(const SnapshotStore::Bytes& bytes);

  /** @brief Encodes states into the snapshot layout (exposed for tests). */
  [[nodiscard]] static std::vector<std::uint8_t> encode(const std::vector<WorldEntity>& entities);

 private:
  void sort_entities();

  std::vector<WorldEntity> entities_;
  std::vector<WorldCommand> pending_;
  std::optional<EnvironmentSpec> environment_;
  SnapshotStore store_;
  std::uint64_t step_index_ = 0;
};

}  // namespace srsp::core

#endif  // SRSP_CORE_WORLD_HPP
