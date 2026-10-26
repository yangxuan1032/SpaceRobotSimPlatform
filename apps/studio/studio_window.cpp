/**
 * @file studio_window.cpp
 * @brief Four-panel studio shell wired to the dynamics and renderer.
 */
#include "studio_window.hpp"

#include <cmath>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace srsp::studio {
namespace {

constexpr double kStepSeconds = 1e-3;

/// Widget that paints the current scene; it asks the window for the renderer.
class Viewport final : public QWidget {
 public:
  explicit Viewport(QtRenderer* renderer, QWidget* parent = nullptr)
      : QWidget(parent), renderer_(renderer) {
    setMinimumSize(480, 320);
  }

  void set_scene(const srsp::render::Scene& scene) {
    scene_ = scene;
    update();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    renderer_->paint(painter, srsp::render::Viewport{width(), height()});
    painter.setPen(QColor(220, 220, 220));
    painter.drawText(
        12, 20, QStringLiteral("scene bodies: %1").arg(static_cast<int>(scene_.bodies.size())));
  }

 private:
  QtRenderer* renderer_ = nullptr;
  srsp::render::Scene scene_{};
};

}  // namespace

StudioWindow::StudioWindow(QtRenderer* renderer, const srsp::model::Model& model, QWidget* parent)
    : QMainWindow(parent), renderer_(renderer) {
  model_ = std::make_unique<srsp::model::Model>(model);
  dynamics_ = std::make_unique<srsp::dynamics::RigidBodyDynamics>(*model_);
  q_ = dynamics_->neutral_configuration();
  v_ = Eigen::VectorXd::Zero(dynamics_->nv());
  tau_ = Eigen::VectorXd::Zero(dynamics_->nv());

  setWindowTitle(QStringLiteral("SRSP Studio"));
  build_panels();
  update_scene();
  refresh_properties();
}

StudioWindow::~StudioWindow() = default;

void StudioWindow::build_panels() {
  auto* viewport = new Viewport(renderer_);
  viewport_ = viewport;

  tree_ = new QTreeWidget();
  tree_->setHeaderLabels({QStringLiteral("Object"), QStringLiteral("Type")});
  auto* bodies = new QTreeWidgetItem(tree_, {QStringLiteral("bodies")});
  bodies->setExpanded(true);
  for (const auto& body : model_->bodies) {
    new QTreeWidgetItem(bodies, {QString::fromStdString(body.id), QStringLiteral("body")});
  }
  auto* joints = new QTreeWidgetItem(tree_, {QStringLiteral("joints")});
  joints->setExpanded(true);
  for (const auto& joint : model_->joints) {
    new QTreeWidgetItem(joints, {QString::fromStdString(joint.id), QStringLiteral("joint")});
  }

  properties_ = new QListWidget();
  timeline_ = new QSlider(Qt::Horizontal);
  timeline_->setRange(0, 60000);
  timeline_->setValue(0);
  step_label_ = new QLabel(QStringLiteral("t = 0.000 s"));

  play_button_ = new QPushButton(QStringLiteral("Play"));
  auto* reset_button = new QPushButton(QStringLiteral("Reset"));
  auto* step_button = new QPushButton(QStringLiteral("Step"));

  connect(play_button_, &QPushButton::clicked, this, [this] { toggle_play(); });
  connect(step_button, &QPushButton::clicked, this, [this] { advance(1); });
  connect(reset_button, &QPushButton::clicked, this, [this] {
    q_ = dynamics_->neutral_configuration();
    v_.setZero();
    steps_ = 0;
    timeline_->setValue(0);
    update_scene();
    refresh_properties();
  });
  connect(timeline_, &QSlider::sliderMoved, this, &StudioWindow::on_timeline_moved);

  auto* timeline_panel = new QWidget();
  auto* timeline_layout = new QHBoxLayout(timeline_panel);
  timeline_layout->addWidget(play_button_);
  timeline_layout->addWidget(step_button);
  timeline_layout->addWidget(reset_button);
  timeline_layout->addWidget(timeline_, 1);
  timeline_layout->addWidget(step_label_);

  auto* splitter = new QSplitter(Qt::Horizontal);
  splitter->addWidget(tree_);
  splitter->addWidget(viewport);
  splitter->addWidget(properties_);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({220, 700, 260});

  auto* central = new QWidget();
  auto* layout = new QVBoxLayout(central);
  layout->addWidget(splitter, 1);
  layout->addWidget(timeline_panel);
  setCentralWidget(central);

  statusBar()->showMessage(QStringLiteral("model: %1").arg(QString::fromStdString(model_->name)));
}

void StudioWindow::advance(int steps) {
  for (int i = 0; i < steps; ++i) {
    dynamics_->step(&q_, &v_, tau_, kStepSeconds);
    ++steps_;
  }
  timeline_->setValue(static_cast<int>(steps_ % 60001));
  step_label_->setText(
      QStringLiteral("t = %1 s").arg(static_cast<double>(steps_) * kStepSeconds, 0, 'f', 3));
  update_scene();
  refresh_properties();
}

bool StudioWindow::toggle_play() {
  playing_ = !playing_;
  play_button_->setText(playing_ ? QStringLiteral("Pause") : QStringLiteral("Play"));
  return playing_;
}

void StudioWindow::on_timeline_moved(int value) {
  // Scrubbing replays from t = 0 deterministically.
  q_ = dynamics_->neutral_configuration();
  v_.setZero();
  steps_ = 0;
  advance(value);
}

void StudioWindow::update_scene() {
  srsp::render::Scene scene;
  scene.has_earth = true;
  scene.earth.position_m[0] = 0.0;
  scene.earth.position_m[1] = 4.0e7;
  scene.earth.position_m[2] = 0.0;
  scene.earth.radius_m = 6.371e6;
  scene.earth.color[0] = 0.15f;
  scene.earth.color[1] = 0.35f;
  scene.earth.color[2] = 0.75f;

  const auto poses = dynamics_->forward_kinematics(q_);
  const auto& ids = dynamics_->body_ids();
  for (std::size_t i = 0; i < poses.size(); ++i) {
    srsp::render::Sphere sphere;
    sphere.position_m[0] = poses[i].position.x();
    sphere.position_m[1] = poses[i].position.y();
    sphere.position_m[2] = poses[i].position.z();
    sphere.radius_m = i == 0 ? 1.2 : 0.35;
    sphere.color[0] = i == 0 ? 0.9f : 0.4f;
    sphere.color[1] = i == 0 ? 0.7f : 0.8f;
    sphere.color[2] = i == 0 ? 0.2f : 0.4f;
    sphere.label = ids[i];
    scene.bodies.push_back(sphere);
  }
  if (viewport_ != nullptr) {
    static_cast<Viewport*>(viewport_)->set_scene(scene);
  }
}

void StudioWindow::refresh_properties() {
  properties_->clear();
  properties_->addItem(QStringLiteral("model: %1").arg(QString::fromStdString(model_->name)));
  properties_->addItem(QStringLiteral("bodies: %1").arg(model_->bodies.size()));
  properties_->addItem(QStringLiteral("joints: %1").arg(model_->joints.size()));
  properties_->addItem(QStringLiteral("dof: %1").arg(dynamics_->nv()));
  properties_->addItem(QStringLiteral("step: %1").arg(steps_));
  properties_->addItem(
      QStringLiteral("time: %1 s").arg(static_cast<double>(steps_) * kStepSeconds));
}

int StudioWindow::tree_body_count() const {
  const auto* root = tree_->topLevelItem(0);
  return root != nullptr ? root->childCount() : 0;
}

int StudioWindow::tree_joint_count() const {
  const auto* root = tree_->topLevelItem(1);
  return root != nullptr ? root->childCount() : 0;
}

int StudioWindow::timeline_maximum() const {
  return timeline_->maximum();
}

QString StudioWindow::properties_text() const {
  QStringList lines;
  for (int i = 0; i < properties_->count(); ++i) {
    lines << properties_->item(i)->text();
  }
  return lines.join('\n');
}

}  // namespace srsp::studio
