/**
 * @file main.cpp
 * @brief srsp-studio entry point and headless self-tests.
 *
 * Modes:
 *   srsp-studio --selftest            renderer contract + offscreen frame
 *   srsp-studio --selftest-ui <srm>   four panels + timeline, offscreen Qt
 *   srsp-studio <srm>                 interactive shell
 */
#include <srsp/model/srm.hpp>
#include <srsp/render/renderer.hpp>
#include <srsp/render/scene.hpp>

#include <cstring>
#include <string>

#include <QApplication>
#include <QSurfaceFormat>
#include <QTimer>
#include <cstdio>
#include <cstdlib>

#include "qt_renderer.hpp"
#include "studio_window.hpp"

namespace {

using srsp::render::CameraView;
using srsp::render::Scene;
using srsp::render::Sphere;
using srsp::render::Viewport;
using srsp::studio::QtRenderer;
using srsp::studio::StudioWindow;

int run_render_selftest() {
  QtRenderer renderer;
  std::string error;
  if (!renderer.initialize(nullptr, Viewport{1280, 720}, &error)) {
    std::fprintf(stderr, "FAIL: initialize: %s\n", error.c_str());
    return 1;
  }

  CameraView camera;
  camera.eye[1] = -20.0;
  camera.far_m = 1.0e9;
  renderer.set_camera(camera);

  Scene scene;
  scene.has_earth = true;
  scene.earth.position_m[1] = 1.0e7;
  scene.earth.radius_m = 6.371e6;
  scene.earth.color[0] = 0.15f;
  scene.earth.color[1] = 0.35f;
  scene.earth.color[2] = 0.75f;
  Sphere satellite;
  satellite.radius_m = 1.5;
  satellite.color[0] = 0.9f;
  satellite.color[1] = 0.7f;
  satellite.color[2] = 0.2f;
  scene.bodies.push_back(satellite);
  renderer.set_scene(scene);

  for (int frame = 0; frame < 3; ++frame) {
    if (!renderer.render_frame()) {
      std::fputs("FAIL: render_frame\n", stderr);
      return 1;
    }
  }

  const QImage image = renderer.render_image(320, 240);
  if (image.isNull()) {
    std::fputs("FAIL: null image\n", stderr);
    return 1;
  }
  int lit = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (image.pixelColor(x, y).value() > 40) {
        ++lit;
      }
    }
  }
  if (lit < 50) {
    std::fprintf(stderr, "FAIL: only %d lit pixels\n", lit);
    return 1;
  }
  renderer.shutdown();
  std::printf("PASS: renderer contract ok (qt backend, %d lit pixels)\n", lit);
  return 0;
}

/// Loads the model path from argv or SRSP_TEST_MODELS.
std::string model_path(int argc, char** argv) {
  if (argc > 2) {
    return argv[2];
  }
  const char* models = std::getenv("SRSP_TEST_MODELS");
  if (models != nullptr) {
    return std::string(models) + "/arm6.srm.json";
  }
  return "examples/models/arm6.srm.json";
}

int run_ui_selftest(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  const std::string path = model_path(argc, argv);
  const auto loaded = srsp::model::load_from_file(path);
  if (!loaded.ok()) {
    std::fprintf(stderr, "FAIL: cannot load %s (%zu issues)\n", path.c_str(), loaded.issues.size());
    return 1;
  }

  QtRenderer renderer;
  std::string error;
  if (!renderer.initialize(nullptr, Viewport{640, 480}, &error)) {
    std::fprintf(stderr, "FAIL: renderer init: %s\n", error.c_str());
    return 1;
  }
  StudioWindow window(&renderer, loaded.model);
  window.resize(1280, 720);

  if (window.tree_body_count() != static_cast<int>(loaded.model.bodies.size())) {
    std::fprintf(stderr, "FAIL: tree shows %d bodies, model has %zu\n", window.tree_body_count(),
                 loaded.model.bodies.size());
    return 1;
  }
  if (window.tree_joint_count() != static_cast<int>(loaded.model.joints.size())) {
    std::fputs("FAIL: joint count in the tree\n", stderr);
    return 1;
  }
  if (window.timeline_maximum() != 60000) {
    std::fputs("FAIL: timeline range\n", stderr);
    return 1;
  }

  window.advance(10);
  if (window.step_count() != 10) {
    std::fprintf(stderr, "FAIL: step count %lld\n", window.step_count());
    return 1;
  }
  if (window.mesh_body_count() <= 0) {
    std::fputs("FAIL: scene has no mesh bodies\n", stderr);
    return 1;
  }
  if (!window.properties_text().contains(QStringLiteral("step: 10"))) {
    std::fputs("FAIL: properties did not update\n", stderr);
    return 1;
  }
  if (!window.toggle_play() || !window.playing()) {
    std::fputs("FAIL: play toggle\n", stderr);
    return 1;
  }
  if (window.toggle_play() || window.playing()) {
    std::fputs("FAIL: pause toggle\n", stderr);
    return 1;
  }

  std::printf("PASS: studio panels wired (bodies=%d, joints=%d, timeline=%d)\n",
              window.tree_body_count(), window.tree_joint_count(), window.timeline_maximum());
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) {
    return run_render_selftest();
  }
  if (argc > 1 && std::strcmp(argv[1], "--selftest-ui") == 0) {
    return run_ui_selftest(argc, argv);
  }

  QSurfaceFormat format;
  format.setDepthBufferSize(24);
  QSurfaceFormat::setDefaultFormat(format);

  QApplication app(argc, argv);

  const std::string path = argc > 1 ? argv[1] : "examples/models/arm6.srm.json";
  const auto loaded = srsp::model::load_from_file(path);
  if (!loaded.ok()) {
    std::fprintf(stderr, "error: cannot load model %s\n", path.c_str());
    return 2;
  }

  QtRenderer renderer;
  StudioWindow window(&renderer, loaded.model);
  window.resize(1280, 720);
  window.show();

  CameraView camera;
  camera.eye[1] = -12.0;
  camera.far_m = 1.0e9;
  renderer.set_camera(camera);

  std::string error;
  if (!renderer.initialize(nullptr, Viewport{window.width(), window.height()}, &error)) {
    std::fprintf(stderr, "error: renderer init failed: %s\n", error.c_str());
    return 2;
  }

  auto* timer = new QTimer(&window);
  QObject::connect(timer, &QTimer::timeout, &window, [&window] {
    if (window.playing()) {
      window.advance(1);
    } else {
      window.update();
    }
  });
  timer->start(16);
  return app.exec();
}
