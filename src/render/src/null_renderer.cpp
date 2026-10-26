/**
 * @file null_renderer.cpp
 * @brief Headless renderer used by tests, CI and `srsp-studio --selftest`.
 */
#include <srsp/render/renderer.hpp>

namespace srsp::render {
namespace {

class NullRenderer final : public Renderer {
 public:
  [[nodiscard]] const char* name() const noexcept override { return "null"; }

  bool initialize(void* native_window, const Viewport& viewport, std::string* error) override {
    if (initialized_) {
      if (error != nullptr) {
        *error = "renderer already initialised";
      }
      return false;
    }
    if (viewport.width <= 0 || viewport.height <= 0) {
      if (error != nullptr) {
        *error = "viewport must be positive";
      }
      return false;
    }
    native_window_ = native_window;
    viewport_ = viewport;
    initialized_ = true;
    frames_ = 0;
    return true;
  }

  bool resize(const Viewport& viewport) override {
    if (!initialized_) {
      return false;
    }
    if (viewport.width <= 0 || viewport.height <= 0) {
      return false;
    }
    viewport_ = viewport;
    return true;
  }

  void set_camera(const CameraView& camera) override {
    camera_ = camera;
    camera_set_ = true;
  }

  bool render_frame() override {
    if (!initialized_) {
      return false;
    }
    ++frames_;
    return true;
  }

  [[nodiscard]] std::uint64_t frame_count() const noexcept override { return frames_; }

  void shutdown() override {
    initialized_ = false;
    native_window_ = nullptr;
    frames_ = 0;
    camera_set_ = false;
  }

  // Helpers used by the studio self-test to assert the bound state.
  [[nodiscard]] bool initialized() const noexcept { return initialized_; }
  [[nodiscard]] const Viewport& viewport() const noexcept { return viewport_; }
  [[nodiscard]] bool camera_set() const noexcept { return camera_set_; }
  [[nodiscard]] void* native_window() const noexcept { return native_window_; }

 private:
  bool initialized_ = false;
  bool camera_set_ = false;
  void* native_window_ = nullptr;
  Viewport viewport_{};
  CameraView camera_{};
  std::uint64_t frames_ = 0;
};

}  // namespace

std::unique_ptr<Renderer> make_null_renderer() {
  return std::make_unique<NullRenderer>();
}

}  // namespace srsp::render
