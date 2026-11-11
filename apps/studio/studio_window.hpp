/**
 * @file studio_window.hpp
 * @brief Studio shell with the four M1 panels: scene tree, properties,
 *        timeline and viewport.
 *
 * The window owns the simulation state so the timeline can advance it; the
 * renderer only receives a Scene every frame. Introspection helpers exist so
 * the headless self-test can assert the panel wiring without a display.
 */
#ifndef SRSP_STUDIO_STUDIO_WINDOW_HPP
#define SRSP_STUDIO_STUDIO_WINDOW_HPP

#include <srsp/dynamics/rigid_body.hpp>
#include <srsp/model/srm.hpp>
#include <srsp/render/scene.hpp>

#include <cstdint>
#include <memory>
#include <string>

#include <QImage>
#include <QMainWindow>
#include <QPoint>
#include <QString>
#include <rhi_renderer.hpp>

#include "qt_renderer.hpp"

class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QTreeWidget;

namespace srsp::studio {

/** @brief Main window: panels + a tiny playback loop over a loaded model. */
class StudioWindow final : public QMainWindow {
 public:
  /**
   * @brief Builds the panels for @p model.
   * @param renderer Renderer used by the viewport widget (not owned).
   * @param model Validated SRM model; dynamics is constructed from it.
   */
  StudioWindow(QtRenderer* renderer, const srsp::model::Model& model, QWidget* parent = nullptr);
  ~StudioWindow() override;

  /** @brief Advances the simulation by @p steps fixed 1 ms steps. */
  void advance(int steps);

  /** @brief Toggles playback; returns the new state. */
  bool toggle_play();

  // Introspection for the headless self-test.
  [[nodiscard]] int tree_body_count() const;
  /** @brief Switches the viewport between the Qt painter and the RHI backend. */
  void set_renderer_mode(bool use_rhi);

  /** @brief Renders one RHI frame into the offscreen target (tests/bench). */
  void render_rhi_once();

  /** @brief True when the hybrid overlay has an RHI frame to blit. */
  [[nodiscard]] bool rhi_overlay_ready() const noexcept { return !rhi_image_.isNull(); }

  /** @brief Frames rendered by the RHI backend (0 when not in use). */
  [[nodiscard]] std::uint64_t rhi_frames() const;

  /** @brief Sets the orbit camera (yaw/pitch in degrees, distance in metres). */
  void set_orbit_camera(double yaw_deg, double pitch_deg, double distance);

  /** @brief Picks the entity under a normalised device coordinate. */
  [[nodiscard]] QString pick_at(double ndc_x, double ndc_y) const;

  /** @brief Bodies rendered with a mesh (procedural box) in the current scene. */
  [[nodiscard]] int mesh_body_count() const;
  [[nodiscard]] int tree_joint_count() const;
  [[nodiscard]] bool playing() const noexcept { return playing_; }
  [[nodiscard]] int timeline_maximum() const;
  [[nodiscard]] long long step_count() const noexcept { return steps_; }
  [[nodiscard]] QString properties_text() const;

 private:
  void build_panels();
  void refresh_properties();
  void update_scene();
  void on_timeline_moved(int value);
  void apply_camera();
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

  QtRenderer* renderer_ = nullptr;
  std::unique_ptr<srsp::render::RhiRenderer> rhi_;
  bool use_rhi_ = false;
  srsp::render::Scene scene_{};
  std::unique_ptr<srsp::model::Model> model_;
  std::unique_ptr<srsp::dynamics::RigidBodyDynamics> dynamics_;
  Eigen::VectorXd q_;
  Eigen::VectorXd v_;
  Eigen::VectorXd tau_;

  QWidget* viewport_ = nullptr;
  QImage rhi_image_;  ///< Last RHI readback, painted under the 2D overlay.
  QTreeWidget* tree_ = nullptr;
  QListWidget* properties_ = nullptr;
  QSlider* timeline_ = nullptr;
  QLabel* step_label_ = nullptr;
  QPushButton* play_button_ = nullptr;
  bool playing_ = false;
  long long steps_ = 0;
  int mesh_bodies_ = 0;
  double yaw_deg_ = 0.0;
  double pitch_deg_ = 0.0;
  double distance_m_ = 12.0;
  bool dragging_ = false;
  QPoint last_mouse_;
  QString picked_;
};

}  // namespace srsp::studio

#endif  // SRSP_STUDIO_STUDIO_WINDOW_HPP
