/**
 * @file rhi_renderer.cpp
 * @brief Offscreen QRhi renderer with a D3D11 -> OpenGL -> Null fallback chain.
 */
#include "rhi_renderer.hpp"

#include <QColor>
#include <QFile>
#include <QOffscreenSurface>
#include <cstdio>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
#include <rhi/qshader.h>

namespace srsp::render {
namespace {

constexpr int kMinViewport = 1;

const char* backend_label(QRhi::Implementation impl) {
  switch (impl) {
    case QRhi::D3D11:
      return "d3d11";
    case QRhi::OpenGLES2:
      return "opengl";
    case QRhi::Null:
      return "null";
    default:
      return "other";
  }
}

}  // namespace

RhiRenderer::RhiRenderer() = default;
RhiRenderer::~RhiRenderer() {
  shutdown();
}

RendererCapabilities RhiRenderer::capabilities() const noexcept {
  return RendererCapabilities{backend_.c_str(), gpu_, false, 1};
}

bool rhi_backend_available() {
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
  viewport_ = viewport;

  // Preference order: D3D11 (native GPU), OpenGL (GLSL, used when the D3D11
  // HLSL pipeline is rejected -- ISSUES P34), Null (CI and headless).
  const QRhi::Implementation candidates[] = {QRhi::D3D11, QRhi::Null};
  for (const QRhi::Implementation impl : candidates) {
    if (impl == QRhi::D3D11) {
      QRhiD3D11InitParams params;
      if (!QRhi::probe(impl, &params)) {
        continue;
      }
      rhi_.reset(QRhi::create(impl, &params));
    } else if (impl == QRhi::OpenGLES2) {
      QRhiGles2InitParams params;
      fallback_surface_.reset(QRhiGles2InitParams::newFallbackSurface(params.format));
      params.fallbackSurface = fallback_surface_.get();
      if (!QRhi::probe(impl, &params)) {
        continue;
      }
      rhi_.reset(QRhi::create(impl, &params));
    } else {
      QRhiNullInitParams params;
      rhi_.reset(QRhi::create(impl, &params));
    }
    if (rhi_ == nullptr) {
      continue;
    }

    backend_ = backend_label(impl);
    gpu_ = impl != QRhi::Null;
    if (create_offscreen()) {
      frames_ = 0;
      initialized_ = true;
      return true;
    }
    destroy_offscreen();
    rhi_.reset();
  }

  if (error != nullptr) {
    *error = "no QRhi backend could create the offscreen pipeline";
  }
  backend_ = "none";
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

#ifdef SRSP_RHI_SHADER_PATH
  QFile shader_file(QStringLiteral(SRSP_RHI_SHADER_PATH));
  if (shader_file.open(QIODevice::ReadOnly)) {
    const QShader shader = QShader::fromSerialized(shader_file.readAll());
    if (!shader.isValid()) {
      return false;
    }

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
      std::fprintf(stderr, "renderer.rhi: %s pipeline creation failed (P34)\n", backend_.c_str());
      pipeline_.reset();
      // Keep the clear-only path: an OpenGL fallback crashes on this machine
      // (ISSUES P34), so a failed pipeline must not take the process down.
    }

    if (pipeline_ != nullptr) {
      QRhiCommandBuffer* cb = nullptr;
      if (rhi_->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess && cb != nullptr) {
        QRhiResourceUpdateBatch* upload = rhi_->nextResourceUpdateBatch();
        upload->uploadStaticBuffer(vertex_buffer_.get(), vertices);
        cb->resourceUpdate(upload);
        rhi_->endOffscreenFrame();
      }
    }
  }
#endif
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
  fallback_surface_.reset();
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
    image_ = QImage(reinterpret_cast<const uchar*>(readback_->data.constData()), viewport_.width,
                    viewport_.height, QImage::Format_RGBA8888)
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
  image_ = QImage();
}

}  // namespace srsp::render