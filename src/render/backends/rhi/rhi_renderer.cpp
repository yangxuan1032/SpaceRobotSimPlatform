/**
 * @file rhi_renderer.cpp
 * @brief Offscreen QRhi renderer: camera-driven sphere on D3D11 with a Null fallback.
 */
#include "rhi_renderer.hpp"

#include <srsp/render/scene.hpp>

#include <cmath>
#include <vector>

#include <QColor>
#include <QFile>
#include <QMatrix4x4>
#include <QOffscreenSurface>
#include <cstdio>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
#include <rhi/qshader.h>

namespace srsp::render {
namespace {

constexpr int kMinViewport = 1;
constexpr int kSlices = 32;
constexpr int kStacks = 16;

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

/// Builds a unit UV sphere: position (3 floats) plus colour (3 floats).
std::vector<float> make_sphere_vertices(float r, float g, float b) {
  std::vector<float> data;
  data.reserve(static_cast<std::size_t>(kSlices + 1) * (kStacks + 1) * 6);
  for (int stack = 0; stack <= kStacks; ++stack) {
    const double v = static_cast<double>(stack) / kStacks;
    const double phi = v * 3.14159265358979323846;
    for (int slice = 0; slice <= kSlices; ++slice) {
      const double u = static_cast<double>(slice) / kSlices;
      const double theta = u * 2.0 * 3.14159265358979323846;
      const double x = std::sin(phi) * std::cos(theta);
      const double y = std::cos(phi);
      const double z = std::sin(phi) * std::sin(theta);
      const auto shade = static_cast<float>(0.55 + 0.45 * std::max(0.0, y));
      data.push_back(static_cast<float>(x));
      data.push_back(static_cast<float>(y));
      data.push_back(static_cast<float>(z));
      data.push_back(r * shade);
      data.push_back(g * shade);
      data.push_back(b * shade);
    }
  }
  return data;
}

std::vector<std::uint16_t> make_sphere_indices() {
  std::vector<std::uint16_t> indices;
  const int stride = kSlices + 1;
  for (int stack = 0; stack < kStacks; ++stack) {
    for (int slice = 0; slice < kSlices; ++slice) {
      const auto a = static_cast<std::uint16_t>(stack * stride + slice);
      const auto b = static_cast<std::uint16_t>(a + 1);
      const auto c = static_cast<std::uint16_t>((stack + 1) * stride + slice);
      const auto d = static_cast<std::uint16_t>(c + 1);
      indices.insert(indices.end(), {a, c, b, b, c, d});
    }
  }
  return indices;
}

/// Appends one oriented box (36 vertices, six shaded faces) in world space.
void append_box(const Sphere& body, std::vector<float>& out) {
  const double* q = body.orientation;
  const double x = q[0];
  const double y = q[1];
  const double z = q[2];
  const double w = q[3];
  const double norm = std::sqrt(x * x + y * y + z * z + w * w);
  const double s = norm > 0.0 ? 1.0 / norm : 1.0;
  const double xx = x * s * x * s;
  const double yy = y * s * y * s;
  const double zz = z * s * z * s;
  const double xy = x * s * y * s;
  const double xz = x * s * z * s;
  const double yz = y * s * z * s;
  const double wx = w * s * x * s;
  const double wy = w * s * y * s;
  const double wz = w * s * z * s;
  const double r[9] = {1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz),       2.0 * (xz + wy),
                       2.0 * (xy + wz),       1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx),
                       2.0 * (xz - wy),       2.0 * (yz + wx),       1.0 - 2.0 * (xx + yy)};
  const double h = body.radius_m;
  const double corners[8][3] = {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
                                {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h}};
  const int faces[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
                           {2, 3, 7, 6}, {1, 2, 6, 5}, {0, 4, 7, 3}};
  const double light[3] = {0.4, 0.6, 0.7};

  for (int f = 0; f < 6; ++f) {
    const double u[3] = {corners[faces[f][1]][0] - corners[faces[f][0]][0],
                         corners[faces[f][1]][1] - corners[faces[f][0]][1],
                         corners[faces[f][1]][2] - corners[faces[f][0]][2]};
    const double v[3] = {corners[faces[f][3]][0] - corners[faces[f][0]][0],
                         corners[faces[f][3]][1] - corners[faces[f][0]][1],
                         corners[faces[f][3]][2] - corners[faces[f][0]][2]};
    double local[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                       u[0] * v[1] - u[1] * v[0]};
    const double length =
        std::sqrt(local[0] * local[0] + local[1] * local[1] + local[2] * local[2]);
    if (length > 0.0) {
      local[0] /= length;
      local[1] /= length;
      local[2] /= length;
    }
    const double normal[3] = {r[0] * local[0] + r[1] * local[1] + r[2] * local[2],
                              r[3] * local[0] + r[4] * local[1] + r[5] * local[2],
                              r[6] * local[0] + r[7] * local[1] + r[8] * local[2]};
    const double shade = 0.35 + 0.65 * std::max(0.0, normal[0] * light[0] + normal[1] * light[1] +
                                                         normal[2] * light[2]);
    for (const int corner :
         {faces[f][0], faces[f][1], faces[f][2], faces[f][0], faces[f][2], faces[f][3]}) {
      const double px = corners[corner][0];
      const double py = corners[corner][1];
      const double pz = corners[corner][2];
      out.push_back(static_cast<float>(body.position_m[0] + r[0] * px + r[1] * py + r[2] * pz));
      out.push_back(static_cast<float>(body.position_m[1] + r[3] * px + r[4] * py + r[5] * pz));
      out.push_back(static_cast<float>(body.position_m[2] + r[6] * px + r[7] * py + r[8] * pz));
      out.push_back(static_cast<float>(body.color[0] * shade));
      out.push_back(static_cast<float>(body.color[1] * shade));
      out.push_back(static_cast<float>(body.color[2] * shade));
    }
  }
}
/// Perspective + look-at matrix for the renderer camera.
QMatrix4x4 build_mvp(const CameraView& camera, const Viewport& viewport) {
  QMatrix4x4 view;
  view.lookAt(QVector3D(static_cast<float>(camera.eye[0]), static_cast<float>(camera.eye[1]),
                        static_cast<float>(camera.eye[2])),
              QVector3D(static_cast<float>(camera.target[0]), static_cast<float>(camera.target[1]),
                        static_cast<float>(camera.target[2])),
              QVector3D(static_cast<float>(camera.up[0]), static_cast<float>(camera.up[1]),
                        static_cast<float>(camera.up[2])));

  QMatrix4x4 projection;
  const auto aspect =
      viewport.height > 0 ? static_cast<float>(viewport.width) / viewport.height : 1.0f;
  projection.perspective(static_cast<float>(camera.fov_deg), aspect,
                         static_cast<float>(camera.near_m), static_cast<float>(camera.far_m));
  return projection * view;
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
  // Mirrors the GPU path: the Null backend always succeeds, so probing it
  // would claim GPU availability on machines (and CI) that have none.
  QRhiD3D11InitParams params;
  return QRhi::probe(QRhi::D3D11, &params);
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

  // A sane default camera so the first frame is never degenerate.
  camera_ = CameraView{};
  camera_.eye[1] = -5.0;
  camera_.up[2] = 1.0;
  camera_.fov_deg = 45.0;
  camera_.near_m = 0.01;
  camera_.far_m = 1.0e9;

  const QRhi::Implementation candidates[] = {QRhi::D3D11, QRhi::Null};
  for (const QRhi::Implementation impl : candidates) {
    if (impl == QRhi::D3D11) {
      QRhiD3D11InitParams params;
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

void RhiRenderer::set_scene(const Scene& scene) {
  scene_ = scene;
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

#ifdef SRSP_RHI_VS_PATH
  const auto load_shader = [](const char* path) {
    QFile file(QString::fromUtf8(path));
    if (!file.open(QIODevice::ReadOnly)) {
      return QShader{};
    }
    return QShader::fromSerialized(file.readAll());
  };
  const QShader vertex_shader = load_shader(SRSP_RHI_VS_PATH);
  const QShader fragment_shader = load_shader(SRSP_RHI_FS_PATH);
  if (!vertex_shader.isValid() || !fragment_shader.isValid()) {
    std::fprintf(stderr, "renderer.rhi: shader load failed (%s, %s)\n", SRSP_RHI_VS_PATH,
                 SRSP_RHI_FS_PATH);
    return false;
  }

  const std::vector<float> vertices = make_sphere_vertices(0.85f, 0.35f, 0.2f);
  const std::vector<std::uint16_t> indices = make_sphere_indices();
  vertex_count_ = static_cast<std::uint32_t>(indices.size());

  vertex_buffer_.reset(rhi_->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                       static_cast<quint32>(vertices.size() * sizeof(float))));
  index_buffer_.reset(
      rhi_->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer,
                      static_cast<quint32>(indices.size() * sizeof(std::uint16_t))));
  uniform_buffer_.reset(rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
  box_buffer_.reset(rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                    36u * 6u * 64u * sizeof(float)));
  if (box_buffer_ == nullptr || !box_buffer_->create()) {
    return false;
  }
  if (!vertex_buffer_->create() || !index_buffer_->create() || !uniform_buffer_->create()) {
    return false;
  }

  bindings_.reset(rhi_->newShaderResourceBindings());
  bindings_->setBindings({QRhiShaderResourceBinding::uniformBuffer(
      0, QRhiShaderResourceBinding::VertexStage, uniform_buffer_.get())});
  if (!bindings_->create()) {
    return false;
  }

  QRhiVertexInputLayout layout;
  layout.setBindings({{6 * sizeof(float)}});
  layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0},
                        {0, 1, QRhiVertexInputAttribute::Float3, 3 * sizeof(float)}});

  pipeline_.reset(rhi_->newGraphicsPipeline());
  pipeline_->setShaderStages(
      {{QRhiShaderStage::Vertex, vertex_shader}, {QRhiShaderStage::Fragment, fragment_shader}});
  pipeline_->setVertexInputLayout(layout);
  pipeline_->setShaderResourceBindings(bindings_.get());
  pipeline_->setRenderPassDescriptor(pass_.get());
  pipeline_->setTopology(QRhiGraphicsPipeline::Triangles);
  pipeline_->setDepthTest(true);
  pipeline_->setDepthWrite(true);
  if (!pipeline_->create()) {
    std::fprintf(stderr, "renderer.rhi: %s pipeline creation failed (P34)\n", backend_.c_str());
    pipeline_.reset();
    return false;
  }

  QRhiCommandBuffer* cb = nullptr;
  if (rhi_->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess && cb != nullptr) {
    QRhiResourceUpdateBatch* upload = rhi_->nextResourceUpdateBatch();
    upload->uploadStaticBuffer(vertex_buffer_.get(), vertices.data());
    upload->uploadStaticBuffer(index_buffer_.get(), indices.data());
    cb->resourceUpdate(upload);
    rhi_->endOffscreenFrame();
  }
#endif
  return true;
}

void RhiRenderer::destroy_offscreen() {
  pipeline_.reset();
  bindings_.reset();
  uniform_buffer_.reset();
  index_buffer_.reset();
  vertex_buffer_.reset();
  readback_.reset();
  target_.reset();
  pass_.reset();
  color_.reset();
  fallback_surface_.reset();
  vertex_count_ = 0;
}

bool RhiRenderer::render_frame() {
  return render_frame(true);
}

bool RhiRenderer::render_frame(bool capture) {
  if (!initialized_ || rhi_ == nullptr) {
    return false;
  }
  QRhiCommandBuffer* cb = nullptr;
  if (rhi_->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess || cb == nullptr) {
    return false;
  }
  QRhiResourceUpdateBatch* updates = rhi_->nextResourceUpdateBatch();

  if (uniform_buffer_ != nullptr && pipeline_ != nullptr) {
    const QMatrix4x4 mvp = build_mvp(camera_, viewport_);
    updates->updateDynamicBuffer(uniform_buffer_.get(), 0, 64, mvp.constData());
  }

  std::vector<float> box_vertices;
  if (!scene_.bodies.empty()) {
    box_vertices.reserve(scene_.bodies.size() * 36u * 6u);
    for (const auto& body : scene_.bodies) {
      append_box(body, box_vertices);
    }
    const quint32 bytes = static_cast<quint32>(box_vertices.size() * sizeof(float));
    if (bytes <= 36u * 6u * 64u * sizeof(float)) {
      updates->updateDynamicBuffer(box_buffer_.get(), 0, bytes, box_vertices.data());
      box_vertices_ = static_cast<std::uint32_t>(box_vertices.size() / 6u);
    }
  }

  const QColor clear(8, 10, 16);
  cb->beginPass(target_.get(), clear, {1.0f, 0}, updates);
  if (!scene_.bodies.empty() && box_vertices_ > 0) {
    cb->setGraphicsPipeline(pipeline_.get());
    cb->setViewport(
        {0.0f, 0.0f, static_cast<float>(viewport_.width), static_cast<float>(viewport_.height)});
    cb->setShaderResources(bindings_.get());
    const QRhiCommandBuffer::VertexInput box_input{box_buffer_.get(), 0};
    cb->setVertexInput(0, 1, &box_input);
    cb->draw(box_vertices_);
  } else if (pipeline_ != nullptr && vertex_buffer_ != nullptr && index_buffer_ != nullptr) {
    cb->setGraphicsPipeline(pipeline_.get());
    cb->setViewport(
        {0.0f, 0.0f, static_cast<float>(viewport_.width), static_cast<float>(viewport_.height)});
    cb->setShaderResources(bindings_.get());
    const QRhiCommandBuffer::VertexInput input{vertex_buffer_.get(), 0};
    cb->setVertexInput(0, 1, &input, index_buffer_.get(), 0, QRhiCommandBuffer::IndexUInt16);
    cb->drawIndexed(vertex_count_);
  }
  cb->endPass();

  if (capture && readback_ != nullptr) {
    QRhiResourceUpdateBatch* read_batch = rhi_->nextResourceUpdateBatch();
    QRhiReadbackDescription read_description(color_.get());
    read_batch->readBackTexture(read_description, readback_.get());
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

void RhiRenderer::wait_idle() {
  if (rhi_ != nullptr) {
    rhi_->finish();
  }
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
