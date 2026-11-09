/**
 * @file main.cpp
 * @brief srsp-studio entry point and headless self-tests.
 *
 * Modes:
 *   srsp-studio --selftest            renderer contract + offscreen frame
 *   srsp-studio --selftest-ui <srm>   four panels + timeline, offscreen Qt
 *   srsp-studio <srm>                 interactive shell
 */
#include <srsp/core_api.h>
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

#if __has_include("rhi_renderer.hpp")
#include "rhi_renderer.hpp"
#define SRSP_STUDIO_HAVE_RHI 1
#endif
#include "studio_window.hpp"

namespace {

using srsp::render::CameraView;
using srsp::render::Scene;
using srsp::render::Sphere;
using srsp::render::Viewport;
using srsp::studio::QtRenderer;
using srsp::studio::StudioWindow;

/// Maps --renderer to srsp_context_config.renderer (0 auto, 1 qt, 2 rhi, 3 null).
int parse_renderer(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--renderer") != 0) {
      continue;
    }
    const std::string value = argv[i + 1];
    if (value == "auto") {
      return 0;
    }
    if (value == "qt") {
      return 1;
    }
    if (value == "rhi") {
      return 2;
    }
    if (value == "null") {
      return 3;
    }
    std::fprintf(stderr, "error: unknown renderer '%s' (auto|qt|rhi|null)\n", value.c_str());
    return -1;
  }
  return 0;
}

/// Creates a context with the selection and checks it round-trips through the
/// ABI, so studio and CLI agree on the one configuration entry (docs/02 15.1).
bool verify_selection(int renderer) {
  srsp_context_config config{};
  config.struct_size = sizeof(srsp_context_config);
  config.renderer = static_cast<uint32_t>(renderer);

  srsp_error err{};
  srsp_context* ctx = nullptr;
  if (srsp_context_create(&config, &ctx, &err) != SRSP_OK) {
    std::fprintf(stderr, "error: context create failed: %s\n",
                 err.message != nullptr ? err.message : "?");
    return false;
  }
  srsp_context_config effective{};
  effective.struct_size = sizeof(srsp_context_config);
  const bool ok = srsp_context_get_config(ctx, &effective, &err) == SRSP_OK &&
                  effective.renderer == static_cast<uint32_t>(renderer);
  srsp_context_destroy(ctx);
  if (!ok) {
    std::fprintf(stderr, "error: renderer selection did not round-trip\n");
    return false;
  }
  return true;
}
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
  // The RHI pass shades from this vector; without it the earth is ambient-only.
  scene.sun_direction[0] = 0.55;
  scene.sun_direction[1] = -0.72;
  scene.sun_direction[2] = 0.42;
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

/// Scene used by the visual check: earth, a satellite and a box body.
Scene make_visual_scene() {
  Scene scene;
  scene.has_earth = true;
  scene.earth.position_m[1] = 1.0e7;
  scene.earth.radius_m = 6.371e6;
  scene.earth.color[0] = 0.15f;
  scene.earth.color[1] = 0.35f;
  scene.earth.color[2] = 0.75f;
  // The RHI pass shades from this vector; without it the earth is ambient-only.
  scene.sun_direction[0] = 0.55;
  scene.sun_direction[1] = -0.72;
  scene.sun_direction[2] = 0.42;

  Sphere satellite;
  satellite.position_m[0] = -2.5;
  satellite.position_m[2] = 0.0;
  satellite.radius_m = 1.5;
  satellite.color[0] = 0.9f;
  satellite.color[1] = 0.7f;
  satellite.color[2] = 0.2f;
  scene.bodies.push_back(satellite);

  Sphere box;
  box.position_m[0] = 4.0;
  box.position_m[2] = 1.2;
  box.radius_m = 1.2;
  box.color[0] = 0.3f;
  box.color[1] = 0.85f;
  box.color[2] = 0.55f;
  box.mesh_uri = "box";
  box.orientation[2] = 0.3826834;
  box.orientation[3] = 0.9238795;
  scene.bodies.push_back(box);

  // Axis markers give the human inspector an orientation and scale reference in
  // both backends: red +X, green +Y, blue +Z, plus a white origin sphere.
  const struct {
    double position[3];
    float color[3];
  } kAxisMarkers[3] = {
      {{6.0, 0.0, 0.0}, {0.92f, 0.22f, 0.22f}},
      {{0.0, 6.0, 0.0}, {0.30f, 0.90f, 0.35f}},
      {{0.0, 0.0, 6.0}, {0.30f, 0.45f, 0.95f}},
  };
  for (const auto& marker : kAxisMarkers) {
    Sphere axis;
    axis.position_m[0] = marker.position[0];
    axis.position_m[1] = marker.position[1];
    axis.position_m[2] = marker.position[2];
    axis.radius_m = 0.35;
    axis.color[0] = marker.color[0];
    axis.color[1] = marker.color[1];
    axis.color[2] = marker.color[2];
    axis.mesh_uri = "box";
    axis.orientation[3] = 1.0;
    scene.bodies.push_back(axis);
  }

  Sphere origin;
  origin.radius_m = 0.45;
  origin.color[0] = 0.95f;
  origin.color[1] = 0.95f;
  origin.color[2] = 0.95f;
  origin.orientation[3] = 1.0;
  scene.bodies.push_back(origin);
  return scene;
}

/// Renders the visual-check scene once and writes it as a PNG.
int run_visual_snapshot(int renderer_code, const std::string& out_path) {
  const Viewport viewport{1280, 720};
  CameraView camera;
  camera.eye[1] = -20.0;
  camera.eye[2] = 4.0;
  camera.up[2] = 1.0;
  camera.fov_deg = 45.0;
  camera.near_m = 0.01;
  camera.far_m = 1.0e9;
  const Scene scene = make_visual_scene();

  QImage image;
  if (renderer_code == 2) {
#if defined(SRSP_STUDIO_HAVE_RHI)
    srsp::render::RhiRenderer rhi;
    std::string error;
    if (!rhi.initialize(nullptr, viewport, &error)) {
      std::fprintf(stderr, "error: rhi init failed: %s\n", error.c_str());
      return 1;
    }
    if (rhi.backend_name() != "d3d11") {
      std::printf("visual snapshot skipped: no GPU backend (%s)\n", rhi.backend_name().c_str());
      rhi.shutdown();
      return 77;
    }
    rhi.set_camera(camera);
    rhi.set_scene(scene);
    rhi.set_overlay_text("SRSP VISUAL CHECK");
    if (!rhi.resize(viewport) || !rhi.render_frame(true)) {
      std::fputs("error: rhi frame failed\n", stderr);
      rhi.shutdown();
      return 1;
    }
    image = rhi.readback();
    rhi.shutdown();
#else
    std::fputs("error: this build has no RHI backend; use --renderer qt\n", stderr);
    return 1;
#endif
  } else {
    QtRenderer qt;
    std::string error;
    if (!qt.initialize(nullptr, viewport, &error)) {
      std::fprintf(stderr, "error: qt init failed: %s\n", error.c_str());
      return 1;
    }
    qt.set_camera(camera);
    qt.set_scene(scene);
    for (int frame = 0; frame < 3; ++frame) {
      (void)qt.render_frame();
    }
    image = qt.render_image(viewport.width, viewport.height);
    qt.shutdown();
  }

  if (image.isNull()) {
    std::fputs("error: snapshot image is empty\n", stderr);
    return 1;
  }
  if (!image.save(QString::fromStdString(out_path), "PNG")) {
    std::fprintf(stderr, "error: cannot write %s\n", out_path.c_str());
    return 1;
  }
  std::printf("visual snapshot: %s (%dx%d)\n", out_path.c_str(), image.width(), image.height());
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
  const int renderer_code = parse_renderer(argc, argv);
  bool use_rhi = renderer_code == 2;
  for (int i = 1; i + 1 < argc; ++i) {
  }
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
  if (use_rhi) {
    window.set_renderer_mode(true);
  }
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
  if (use_rhi) {
    // Offscreen platform: a hidden widget never paints, so drive the RHI path
    // directly instead of relying on paintEvent.
    window.render_rhi_once();
  }
  if (use_rhi && window.rhi_frames() == 0) {
    std::fputs("FAIL: rhi renderer produced no frames\n", stderr);
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
  const int renderer_code = parse_renderer(argc, argv);
  if (renderer_code < 0) {
    return 1;
  }
  if (!verify_selection(renderer_code)) {
    return 1;
  }
  if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) {
    return run_render_selftest();
  }
  if (argc > 2 && std::strcmp(argv[1], "--snapshot") == 0) {
    return run_visual_snapshot(renderer_code, argv[2]);
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
