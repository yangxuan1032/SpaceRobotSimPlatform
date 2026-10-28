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

#include <memory>
#include <string>

#include <QMainWindow>

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

  QtRenderer* renderer_ = nullptr;
  std::unique_ptr<srsp::model::Model> model_;
  std::unique_ptr<srsp::dynamics::RigidBodyDynamics> dynamics_;
  Eigen::VectorXd q_;
  Eigen::VectorXd v_;
  Eigen::VectorXd tau_;

  QWidget* viewport_ = nullptr;
  QTreeWidget* tree_ = nullptr;
  QListWidget* properties_ = nullptr;
  QSlider* timeline_ = nullptr;
  QLabel* step_label_ = nullptr;
  QPushButton* play_button_ = nullptr;
  bool playing_ = false;
  long long steps_ = 0;
  int mesh_bodies_ = 0;
};

}  // namespace srsp::studio

#endif  // SRSP_STUDIO_STUDIO_WINDOW_HPP
