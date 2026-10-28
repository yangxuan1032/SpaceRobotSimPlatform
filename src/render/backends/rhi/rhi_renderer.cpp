/**
 * @file rhi_renderer.cpp
 * @brief Offscreen QRhi renderer: clear + readback, D3D11 first with Null fallback.
 */
#include "rhi_renderer.hpp"

#include <cstring>

#include <QColor>
#include <QFile>
#include <cstdio>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
#include <rhi/qshader.h>

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
  QRhiD3D11InitParams d3d_params;
  if (QRhi::probe(QRhi::D3D11, &d3d_params)) {
    return true;
  }
  QRhiNullInitParams null_params;
  return QRhi::probe(QRhi::Null, &null_params);
}

bool RhiRenderer::initialize(void* /*native_window*/, const Viewport& viewport,
                             std::string* error) {
  if (initialized_) {
    if (error != nullptr) {
      *error = "renderer already initialised";
    }
    return false;
  }
  if (viewport.width < kMinViewport || viewport.height < kMinViewport) {
    if (error != nullptr) {
      *error = "viewport must be positive";
    }
    return false;
  }

  // D3D11 first: QRhiD3D11InitParams is declared in rhi/qrhi_platform.h and
  // must be non-null (passing nullptr crashes inside QRhi, see ISSUES P33).
  QRhiD3D11InitParams d3d_params;
#if defined(QT_DEBUG)
  d3d_params.enableDebugLayer = true;
#endif
  if (QRhi::probe(QRhi::D3D11, &d3d_params)) {
    rhi_.reset(QRhi::create(QRhi::D3D11, &d3d_params));
  }
  if (rhi_ != nullptr) {
    backend_ = "d3d11";
    gpu_ = true;
  } else {
    // Fallback for headless machines and CI: the Null backend exercises the
    // same pipeline without a device.
    QRhiNullInitParams null_params;
    rhi_.reset(QRhi::create(QRhi::Null, &null_params));
    if (rhi_ == nullptr) {
      if (error != nullptr) {
        *error = "no QRhi backend available (tried d3d11 and null)";
      }
      return false;
    }
    backend_ = "null";
    gpu_ = false;
  }

  viewport_ = viewport;
  if (!create_offscreen()) {
    if (error != nullptr) {
      *error = "failed to create the offscreen render target";
    }
    shutdown();
    return false;
  }
  frames_ = 0;
  initialized_ = true;
  return true;
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

  // One triangle in clip space: proves the pipeline, shaders and draw call.
  const float vertices[] = {
      -0.8f, -0.8f, 1.0f, 0.2f, 0.2f,  //
      0.8f,  -0.8f, 0.2f, 1.0f, 0.2f,  //
      0.0f,  0.8f,  0.2f, 0.2f, 1.0f,  //
  };
  vertex_buffer_.reset(
      rhi_->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertices)));
  if (!vertex_buffer_->create()) {
    return false;
  }

  bindings_.reset(rhi_->newShaderResourceBindings());
  if (!bindings_->create()) {
    return false;
  }

#ifdef SRSP_RHI_SHADER_PATH
  QFile shader_file(QStringLiteral(SRSP_RHI_SHADER_PATH));
  if (!shader_file.open(QIODevice::ReadOnly)) {
    return false;
  }
  const QShader shader = QShader::fromSerialized(shader_file.readAll());
  if (!shader.isValid()) {
    return false;
  }

  QRhiVertexInputLayout layout;
  layout.setBindings({{5 * sizeof(float)}});
  layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0},
                        {0, 1, QRhiVertexInputAttribute::Float3, 2 * sizeof(float)}});

  pipeline_.reset(rhi_->newGraphicsPipeline());
  pipeline_->setShaderStages(
      {{QRhiShaderStage::Vertex, shader}, {QRhiShaderStage::Fragment, shader}});
  pipeline_->setVertexInputLayout(layout);
  pipeline_->setShaderResourceBindings(bindings_.get());
  pipeline_->setRenderPassDescriptor(pass_.get());
  pipeline_->setTopology(QRhiGraphicsPipeline::Triangles);
  if (!pipeline_->create()) {
    // D3D11 rejects the shader on some setups (COM error 0x80070057, P34);
    // fall back to the clear-only path instead of failing initialisation.
    std::fprintf(stderr, "renderer.rhi: pipeline creation failed; drawing only the clear pass\\n");
    pipeline_.reset();
  }
#endif

  // Upload the vertices once.
  QRhiCommandBuffer* cb = nullptr;
  if (rhi_->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess || cb == nullptr) {
    return false;
  }
  QRhiResourceUpdateBatch* upload = rhi_->nextResourceUpdateBatch();
  upload->uploadStaticBuffer(vertex_buffer_.get(), vertices);
  cb->resourceUpdate(upload);
  rhi_->endOffscreenFrame();
  return true;
}

void RhiRenderer::destroy_offscreen() {
  pipeline_.reset();
  bindings_.reset();
  vertex_buffer_.reset();
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
  if (pipeline_ != nullptr && vertex_buffer_ != nullptr) {
    cb->setGraphicsPipeline(pipeline_.get());
    cb->setViewport(
        {0.0f, 0.0f, static_cast<float>(viewport_.width), static_cast<float>(viewport_.height)});
    cb->setShaderResources(bindings_.get());
    const QRhiCommandBuffer::VertexInput input{vertex_buffer_.get(), 0};
    cb->setVertexInput(0, 1, &input);
    cb->draw(3);
  }
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
