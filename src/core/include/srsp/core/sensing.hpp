/**
 * @file sensing.hpp
 * @brief W7/W8: sensor sampling, orbit camera control and entity picking.
 *
 * Everything here is pure math over the world state: camera pose from an entity
 * attitude, a lidar ray sweep against spheres, a range/Doppler radar model, and
 * a mouse ray pick. Rendering consumes the same results, so a camera sensor and
 * the studio view can never disagree about framing.
 */
#ifndef SRSP_CORE_SENSING_HPP
#define SRSP_CORE_SENSING_HPP

#include <srsp/core/world.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <cstddef>

namespace srsp::core {

/** @brief Camera intrinsics shared by the renderer and the camera sensor. */
struct CameraIntrinsics {
  double fov_deg = 45.0;
  double aspect = 16.0 / 9.0;
  double near_m = 0.01;
  double far_m = 1.0e9;
};

/** @brief One sensor declaration: which entity, what kind, at which rate. */
struct SensorMount {
  std::string entity_id;  ///< Host entity.
  std::string kind;       ///< "camera", "lidar" or "radar".
  double rate_hz = 1.0;   ///< Sampling rate; <= 0 disables the sensor.
  double phase_s = 0.0;   ///< Offset so sensors do not fire together.
};

/** @brief A sample that became due at @p t_s. */
struct SensorSample {
  std::string entity_id;
  std::string kind;
  double t_s = 0.0;
};

/**
 * @brief Deterministic sampling schedule driven by simulated time.
 *
 * Rates are turned into periods; the scheduler fires when the accumulated time
 * crosses the next period plus phase. It never reads a wall clock, so the same
 * inputs produce the same sample times in a replay.
 */
class SensorScheduler {
 public:
  /// Registers the sensors; entries with rate <= 0 are ignored.
  explicit SensorScheduler(std::vector<SensorMount> sensors);

  /// Advances simulated time and returns the samples that became due.
  std::vector<SensorSample> advance(double dt);

  /// Total simulated time advanced so far.
  [[nodiscard]] double time_s() const noexcept { return time_s_; }

 private:
  struct Entry {
    SensorMount mount;
    double next_s = 0.0;
  };
  std::vector<Entry> entries_;
  double time_s_ = 0.0;
};
/** @brief Camera placement in the inertial frame. */
struct CameraPose {
  Vec3 eye{};
  Vec3 target{};
  Vec3 up{0.0, 1.0, 0.0};
  bool valid = false;
};

/** @brief Orbit-style camera control state (W8). */
struct OrbitCamera {
  Vec3 target{};
  double distance = 5.0;
  double yaw_deg = 0.0;
  double pitch_deg = 0.0;
  double fov_deg = 45.0;
};

/** @brief Lidar parameters. */
struct LidarConfig {
  int rings = 8;
  int azimuth_samples = 64;
  double max_range_m = 100.0;
  double vertical_fov_deg = 30.0;
};

/** @brief One lidar return. */
struct LidarPoint {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float intensity = 0.0f;
};

/** @brief Radar parameters. */
struct RadarConfig {
  double max_range_m = 1000.0;
};

/** @brief One radar detection (range/Doppler approximation, not full RF). */
struct RadarDetection {
  double range_m = 0.0;
  double radial_velocity_m_s = 0.0;
  double rcs_m2 = 0.0;
};

/**
 * @brief Places a camera behind an entity (body +Z) looking at it.
 * @param entity Observer entity; its attitude defines the frame.
 * @param distance Eye distance along body +Z.
 */
[[nodiscard]] CameraPose camera_pose_from_entity(const WorldEntity& entity, double distance,
                                                 const Vec3& offset_body = {});

/** @brief Pose from an orbit camera (yaw/pitch/distance around a target). */
[[nodiscard]] CameraPose orbit_camera_pose(const OrbitCamera& camera);

/**
 * @brief Sweeps a lidar and returns hits within range.
 * @param config Ring/azimuth sampling and range gate.
 * @param pose Sensor pose (eye/target/up define the scan frame).
 * @param targets World entities treated as spheres.
 */
[[nodiscard]] std::vector<LidarPoint> lidar_scan(const LidarConfig& config, const CameraPose& pose,
                                                 const std::vector<WorldEntity>& targets);

/**
 * @brief Range/Doppler detections of the other entities.
 * @param config Range gate.
 * @param observer Entity carrying the radar.
 * @param targets Everything else in the world.
 */
[[nodiscard]] std::vector<RadarDetection> radar_scan(const RadarConfig& config,
                                                     const WorldEntity& observer,
                                                     const std::vector<WorldEntity>& targets);

/**
 * @brief Picks the nearest entity under a normalised device coordinate.
 * @param pose Camera pose.
 * @param intrinsics Field of view and aspect.
 * @param ndc_x Horizontal NDC in [-1, 1].
 * @param ndc_y Vertical NDC in [-1, 1].
 * @param targets World entities treated as spheres.
 * @return The hit entity id, or empty.
 */
[[nodiscard]] std::optional<std::string> pick_entity(const CameraPose& pose,
                                                     const CameraIntrinsics& intrinsics,
                                                     double ndc_x, double ndc_y,
                                                     const std::vector<WorldEntity>& targets);

}  // namespace srsp::core

#endif  // SRSP_CORE_SENSING_HPP
