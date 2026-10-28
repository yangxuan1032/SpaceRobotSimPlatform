/**
 * @file rhi_renderer.cpp
 * @brief Offscreen QRhi renderer: clear + readback, D3D11 first with Null fallback.
 */
#include "rhi_renderer.hpp"

#include <cstring>

#include <QColor>
#include <rhi/qrhi.h>

namespace srsp::render {
namespace {

constexpr int kMinViewport = 1;

}  // namespace

RhiRenderer::RhiRenderer() = default;
RhiRenderer::~RhiRenderer() {
  shutdown();
}

RendererCapabilities RhiRenderer::capabilities() const noexcept {
  return RendererCapabilities{backend_.c_str(), gpu_, false, 1};
}

bool rhi_backend_available() {
  // Probing would hit the same init-params problem as initialize().
  return false;
}

bool RhiRenderer::initialize(void* /*native_window*/, const Viewport& viewport,
                             std::string* error) {
  // D3D11/Null creation needs the backend-specific init params
  // (QRhiD3D11InitParams / QRhiNullInitParams); passing nullptr crashes inside
  // QRhi (access violation, observed on Qt 6.11). Until the params are wired
  // and verified, fail explicitly instead of taking the process down.
  if (viewport.width < kMinViewport || viewport.height < kMinViewport) {
    if (error != nullptr) {
      *error = "viewport must be positive";
    }
    return false;
  }
  if (error != nullptr) {
    *error =
        "renderer.rhi: backend init params not wired yet "
        "(tracked as ISSUES P33; docs/07 U10)";
  }
  backend_ = "unavailable";
  gpu_ = false;
  return false;
}
bool RhiRenderer::resize(const Viewport& viewport) {
  if (!initialized_ || viewport.width < kMinViewport || viewport.height < kMinViewport) {
    return false;
  }
  viewport_ = viewport;
  destroy_offscreen();
  return create_offscreen();
}

void RhiRenderer::set_camera(const CameraView& camera) {
  camera_ = camera;
}

bool RhiRenderer::create_offscreen() {
  if (rhi_ == nullptr) {
    return false;
  }
  color_.reset(rhi_->newTexture(QRhiTexture::RGBA8, QSize(viewport_.width, viewport_.height), 1,
                                QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
  if (!color_->create()) {
    return false;
  }

  QRhiTextureRenderTargetDescription description{QRhiColorAttachment(color_.get())};
  target_.reset(rhi_->newTextureRenderTarget(description));
  pass_.reset(target_->newCompatibleRenderPassDescriptor());
  target_->setRenderPassDescriptor(pass_.get());
  if (!target_->create()) {
    return false;
  }
  readback_ = std::make_unique<QRhiReadbackResult>();
  return true;
}

void RhiRenderer::destroy_offscreen() {
  readback_.reset();
  target_.reset();
  pass_.reset();
  color_.reset();
}

bool RhiRenderer::render_frame() {
  if (!initialized_ || rhi_ == nullptr) {
    return false;
  }
  QRhiCommandBuffer* cb = nullptr;
  if (rhi_->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess || cb == nullptr) {
    return false;
  }
  QRhiResourceUpdateBatch* updates = rhi_->nextResourceUpdateBatch();
  const QColor clear(8, 10, 16);
  cb->beginPass(target_.get(), clear, {1.0f, 0}, updates);
  cb->endPass();

  if (readback_ != nullptr) {
    QRhiResourceUpdateBatch* read_batch = rhi_->nextResourceUpdateBatch();
    QRhiReadbackDescription description(color_.get());
    read_batch->readBackTexture(description, readback_.get());
    cb->resourceUpdate(read_batch);
  }
  rhi_->endOffscreenFrame();

  if (readback_ != nullptr && !readback_->data.isEmpty()) {
    const QImage::Format format = QImage::Format_RGBA8888;
    image_ = QImage(reinterpret_cast<const uchar*>(readback_->data.constData()), viewport_.width,
                    viewport_.height, format)
                 .copy();
  }
  ++frames_;
  return true;
}

QImage RhiRenderer::readback() const {
  return image_;
}

void RhiRenderer::shutdown() {
  destroy_offscreen();
  rhi_.reset();
  backend_ = "none";
  gpu_ = false;
  initialized_ = false;
  frames_ = 0;
}

}  // namespace srsp::render
