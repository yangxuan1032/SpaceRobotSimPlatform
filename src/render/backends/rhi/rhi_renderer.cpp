/**
 * @file rhi_renderer.cpp
 * @brief Offscreen QRhi renderer: camera-driven sphere on D3D11 with a Null fallback.
 */
#include "rhi_renderer.hpp"

#include <srsp/render/mesh_loader.hpp>
#include <srsp/render/mesh_source.hpp>
#include <srsp/render/scene.hpp>
#include <srsp/render/text_mesh.hpp>

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
/// Dynamic vertex buffer capacity: bodies plus one planet, in floats.
constexpr quint32 kVertexCapacityFloats = 400000;
/// Enough for a few hundred glyph quads (5 floats each).
constexpr quint32 kTextVertexCapacity = 4096;
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

/// Appends a world-space UV sphere as non-indexed triangles.
void append_sphere_world(const Sphere& body, const double sun[3], double ambient,
                         std::vector<float>& out) {
  const int slices = 48;
  const int stacks = 24;
  const double radius = body.radius_m;
  const auto point = [&](int stack, int slice, double xyz[3]) {
    const double phi = 3.14159265358979323846 * static_cast<double>(stack) / stacks;
    const double theta = 2.0 * 3.14159265358979323846 * static_cast<double>(slice) / slices;
    xyz[0] = body.position_m[0] + radius * std::sin(phi) * std::cos(theta);
    xyz[1] = body.position_m[1] + radius * std::cos(phi);
    xyz[2] = body.position_m[2] + radius * std::sin(phi) * std::sin(theta);
  };

  for (int stack = 0; stack < stacks; ++stack) {
    for (int slice = 0; slice < slices; ++slice) {
      double a[3];
      double b[3];
      double cpt[3];
      double d[3];
      point(stack, slice, a);
      point(stack + 1, slice, b);
      point(stack, slice + 1, cpt);
      point(stack + 1, slice + 1, d);
      const auto lit = [&](const double* p) {
        const double nx = (p[0] - body.position_m[0]) / radius;
        const double ny = (p[1] - body.position_m[1]) / radius;
        const double nz = (p[2] - body.position_m[2]) / radius;
        const double d = nx * sun[0] + ny * sun[1] + nz * sun[2];
        return static_cast<float>((ambient + (1.0 - ambient) * std::max(0.0, d)));
      };
      const float s0 = lit(a);
      const float s1 = lit(b);
      const double* quad[6] = {a, b, cpt, cpt, b, d};
      const float shades[6] = {s0, s1, s0, s0, s1, s1};
      for (int i = 0; i < 6; ++i) {
        out.push_back(static_cast<float>(quad[i][0]));
        out.push_back(static_cast<float>(quad[i][1]));
        out.push_back(static_cast<float>(quad[i][2]));
        out.push_back(body.color[0] * shades[i]);
        out.push_back(body.color[1] * shades[i]);
        out.push_back(body.color[2] * shades[i]);
      }
    }
  }
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
/// Emits the triangles of a loaded mesh with flat per-face shading.
void append_mesh(const Sphere& body, const srsp::render::MeshData& mesh, std::vector<float>& out) {
  const double* q = body.orientation;
  const double norm = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  const double s = norm > 0.0 ? 1.0 / norm : 1.0;
  const double x = q[0] * s;
  const double y = q[1] * s;
  const double z = q[2] * s;
  const double w = q[3] * s;
  const double r[9] = {
      1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - w * z),       2.0 * (x * z + w * y),
      2.0 * (x * y + w * z),       1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - w * x),
      2.0 * (x * z - w * y),       2.0 * (y * z + w * x),       1.0 - 2.0 * (x * x + y * y)};
  const double light[3] = {0.4, 0.6, 0.7};

  for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    double world[3][3];
    for (int corner = 0; corner < 3; ++corner) {
      const std::size_t index = mesh.indices[i + static_cast<std::size_t>(corner)];
      const double local[3] = {mesh.positions[index * 3u], mesh.positions[index * 3u + 1u],
                               mesh.positions[index * 3u + 2u]};
      for (int axis = 0; axis < 3; ++axis) {
        world[corner][axis] = body.position_m[axis] + r[axis * 3] * local[0] +
                              r[axis * 3 + 1] * local[1] + r[axis * 3 + 2] * local[2];
      }
    }
    const double e1[3] = {world[1][0] - world[0][0], world[1][1] - world[0][1],
                          world[1][2] - world[0][2]};
    const double e2[3] = {world[2][0] - world[0][0], world[2][1] - world[0][1],
                          world[2][2] - world[0][2]};
    double normal[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                        e1[0] * e2[1] - e1[1] * e2[0]};
    const double length =
        std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (length > 1e-12) {
      normal[0] /= length;
      normal[1] /= length;
      normal[2] /= length;
    }
    const double shade = 0.35 + 0.65 * std::max(0.0, normal[0] * light[0] + normal[1] * light[1] +
                                                         normal[2] * light[2]);
    for (int corner = 0; corner < 3; ++corner) {
      out.push_back(static_cast<float>(world[corner][0]));
      out.push_back(static_cast<float>(world[corner][1]));
      out.push_back(static_cast<float>(world[corner][2]));
      out.push_back(static_cast<float>(body.color[0] * shade));
      out.push_back(static_cast<float>(body.color[1] * shade));
      out.push_back(static_cast<float>(body.color[2] * shade));
    }
  }
}
/// Deterministic star field: 400 points on a sphere far outside the scene.
std::vector<float> make_star_vertices() {
  std::vector<float> data;
  data.reserve(400 * 6);
  std::uint32_t state = 0x12345678u;
  const auto next = [&state]() {
    state = state * 1664525u + 1013904223u;
    return static_cast<double>(state) / 4294967296.0;
  };
  for (int i = 0; i < 400; ++i) {
    const double z = 2.0 * next() - 1.0;
    const double phi = 2.0 * 3.14159265358979323846 * next();
    const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
    const double radius = 1.0e6;
    const float brightness = static_cast<float>(0.4 + 0.6 * next());
    data.push_back(static_cast<float>(radius * r * std::cos(phi)));
    data.push_back(static_cast<float>(radius * r * std::sin(phi)));
    data.push_back(static_cast<float>(radius * z));
    data.push_back(brightness);
    data.push_back(brightness);
    data.push_back(brightness);
  }
  return data;
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

void RhiRenderer::set_overlay_text(std::string text) {
  overlay_text_ = std::move(text);
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
                                    kVertexCapacityFloats * sizeof(float)));
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

  const std::vector<float> stars = make_star_vertices();
  star_buffer_.reset(rhi_->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                     static_cast<quint32>(stars.size() * sizeof(float))));
  if (!star_buffer_->create()) {
    return false;
  }

  star_pipeline_.reset(rhi_->newGraphicsPipeline());
  star_pipeline_->setShaderStages(
      {{QRhiShaderStage::Vertex, vertex_shader}, {QRhiShaderStage::Fragment, fragment_shader}});
  star_pipeline_->setVertexInputLayout(layout);
  star_pipeline_->setShaderResourceBindings(bindings_.get());
  star_pipeline_->setRenderPassDescriptor(pass_.get());
  star_pipeline_->setTopology(QRhiGraphicsPipeline::Points);
  star_pipeline_->setDepthTest(false);
  star_pipeline_->setDepthWrite(false);
  if (!star_pipeline_->create()) {
    star_pipeline_.reset();
  }

#if defined(SRSP_TEXT_VS_PATH) && defined(SRSP_TEXT_FS_PATH)
  // B12 text pass: an R8 coverage atlas sampled by a dedicated pipeline. Any
  // failure here disables text only; geometry rendering must keep working.
  const QShader text_vs = load_shader(SRSP_TEXT_VS_PATH);
  const QShader text_fs = load_shader(SRSP_TEXT_FS_PATH);
  if (text_vs.isValid() && text_fs.isValid()) {
    atlas_data_ = srsp::render::build_text_atlas();
    atlas_texture_.reset(
        rhi_->newTexture(QRhiTexture::RGBA8, QSize(atlas_data_.width, atlas_data_.height), 1));
    atlas_sampler_.reset(rhi_->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest,
                                          QRhiSampler::None, QRhiSampler::ClampToEdge,
                                          QRhiSampler::ClampToEdge));
    text_buffer_.reset(rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                       kTextVertexCapacity * 5u * sizeof(float)));
    text_uniform_.reset(rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16));

    bool text_ok = atlas_texture_ != nullptr && atlas_texture_->create() &&
                   atlas_sampler_ != nullptr && atlas_sampler_->create() &&
                   text_buffer_ != nullptr && text_buffer_->create() && text_uniform_ != nullptr &&
                   text_uniform_->create();
    if (text_ok) {
      text_bindings_.reset(rhi_->newShaderResourceBindings());
      text_bindings_->setBindings(
          {QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage,
                                                    text_uniform_.get()),
           QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                     atlas_texture_.get(), atlas_sampler_.get())});
      text_ok = text_bindings_->create();
    }
    if (text_ok) {
      QRhiVertexInputLayout text_layout;
      text_layout.setBindings({{5 * sizeof(float)}});
      text_layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0},
                                 {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)},
                                 {0, 2, QRhiVertexInputAttribute::Float, 4 * sizeof(float)}});
      QRhiGraphicsPipeline::TargetBlend blend;
      blend.enable = true;
      blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
      blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
      blend.srcAlpha = QRhiGraphicsPipeline::One;
      blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
      text_pipeline_.reset(rhi_->newGraphicsPipeline());
      text_pipeline_->setShaderStages(
          {{QRhiShaderStage::Vertex, text_vs}, {QRhiShaderStage::Fragment, text_fs}});
      text_pipeline_->setVertexInputLayout(text_layout);
      text_pipeline_->setShaderResourceBindings(text_bindings_.get());
      text_pipeline_->setRenderPassDescriptor(pass_.get());
      text_pipeline_->setTopology(QRhiGraphicsPipeline::Triangles);
      text_pipeline_->setDepthTest(false);
      text_pipeline_->setDepthWrite(false);
      text_pipeline_->setTargetBlends({blend});
      text_ok = text_pipeline_->create();
      if (!text_ok) {
        std::fprintf(stderr, "renderer.rhi: text pipeline creation failed (B12)\n");
      }
    }
    if (!text_ok) {
      text_pipeline_.reset();
      text_bindings_.reset();
      text_uniform_.reset();
      text_buffer_.reset();
      atlas_sampler_.reset();
      atlas_texture_.reset();
    }
  }
#endif
  QRhiCommandBuffer* cb = nullptr;
  if (rhi_->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess && cb != nullptr) {
    QRhiResourceUpdateBatch* upload = rhi_->nextResourceUpdateBatch();
    upload->uploadStaticBuffer(vertex_buffer_.get(), vertices.data());
    upload->uploadStaticBuffer(index_buffer_.get(), indices.data());
    upload->uploadStaticBuffer(star_buffer_.get(), stars.data());
    if (atlas_texture_ != nullptr && !atlas_data_.pixels.empty()) {
      // R8 uploads of a grayscale image sampled as 1.0 on D3D11; an explicit
      // white+alpha image keeps the glyph coverage intact on every backend.
      QImage atlas_image(atlas_data_.width, atlas_data_.height, QImage::Format_RGBA8888);
      for (int y = 0; y < atlas_data_.height; ++y) {
        for (int x = 0; x < atlas_data_.width; ++x) {
          const std::uint8_t coverage =
              atlas_data_.pixels[static_cast<std::size_t>(y) * atlas_data_.width + x];
          atlas_image.setPixelColor(x, y, QColor(255, 255, 255, coverage));
        }
      }
      upload->uploadTexture(atlas_texture_.get(), atlas_image);
    }
    cb->resourceUpdate(upload);
    rhi_->endOffscreenFrame();
  }
#endif
  return true;
}

void RhiRenderer::destroy_offscreen() {
  pipeline_.reset();
  text_pipeline_.reset();
  text_bindings_.reset();
  text_uniform_.reset();
  text_buffer_.reset();
  atlas_sampler_.reset();
  atlas_texture_.reset();
  text_vertices_ = 0;
  bindings_.reset();
  uniform_buffer_.reset();
  star_pipeline_.reset();
  star_buffer_.reset();
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
  if (scene_.has_earth) {
    const double ambient = scene_.eclipse ? 0.08 : 0.18;
    append_sphere_world(scene_.earth, scene_.sun_direction, ambient, box_vertices);
  }
  if (!scene_.bodies.empty()) {
    box_vertices.reserve(box_vertices.size() + scene_.bodies.size() * 36u * 6u);
    for (const auto& body : scene_.bodies) {
      switch (srsp::render::mesh_source_from_uri(body.mesh_uri)) {
        case srsp::render::MeshSource::kBox:
          append_box(body, box_vertices);
          break;
        case srsp::render::MeshSource::kSphere:
          append_sphere_world(body, scene_.sun_direction, scene_.eclipse ? 0.08 : 0.18,
                              box_vertices);
          break;
        case srsp::render::MeshSource::kGltf: {
          auto& cached = mesh_cache_[body.mesh_uri];
          if (cached.empty()) {
            const srsp::render::MeshLoadResult loaded = srsp::render::load_glb(body.mesh_uri);
            if (loaded.ok()) {
              cached = loaded.mesh;
            } else {
              static bool warned = false;
              if (!warned) {
                std::fprintf(stderr, "warn: %s; drawing a sphere instead\n", loaded.error.c_str());
                warned = true;
              }
            }
          }
          if (!cached.empty()) {
            append_mesh(body, cached, box_vertices);
          } else {
            append_sphere_world(body, scene_.sun_direction, scene_.eclipse ? 0.08 : 0.18,
                                box_vertices);
          }
          break;
        }
      }
    }
  }
  // Upload whatever the frame built (earth alone is a valid scene).
  box_vertices_ = 0;
  if (!box_vertices.empty()) {
    const quint32 bytes = static_cast<quint32>(box_vertices.size() * sizeof(float));
    if (bytes <= kVertexCapacityFloats * sizeof(float)) {
      updates->updateDynamicBuffer(box_buffer_.get(), 0, bytes, box_vertices.data());
      box_vertices_ = static_cast<std::uint32_t>(box_vertices.size() / 6u);
    }
  }

  if (qEnvironmentVariableIsSet("SRSP_RHI_DEBUG")) {
    std::fprintf(stderr, "rhi: bodies=%zu has_earth=%d eclipse=%d verts=%zu drawn=%u\n",
                 scene_.bodies.size(), scene_.has_earth ? 1 : 0, scene_.eclipse ? 1 : 0,
                 box_vertices.size() / 6u, box_vertices_);
    std::fprintf(stderr, "rhi: text verts=%u pipeline=%d\n", text_vertices_,
                 text_pipeline_ != nullptr ? 1 : 0);
  }

  // B12: build the overlay text quads and stage them in the same batch as the
  // geometry, so the draw below only binds and draws.
  text_vertices_ = 0;
  if (text_pipeline_ != nullptr && text_buffer_ != nullptr && !overlay_text_.empty()) {
    std::vector<float> text_quads;
    const std::size_t emitted = srsp::render::build_text_quads(atlas_data_, overlay_text_, 12.0f,
                                                               12.0f, 3.0f, 255, &text_quads);
    if (emitted > 0 && text_quads.size() / 5u <= kTextVertexCapacity) {
      const quint32 text_bytes = static_cast<quint32>(text_quads.size() * sizeof(float));
      updates->updateDynamicBuffer(text_buffer_.get(), 0, text_bytes, text_quads.data());
      const float text_uniform_data[4] = {static_cast<float>(viewport_.width),
                                          static_cast<float>(viewport_.height), 0.0f, 0.0f};
      updates->updateDynamicBuffer(text_uniform_.get(), 0, 16, text_uniform_data);
      text_vertices_ = static_cast<std::uint32_t>(text_quads.size() / 5u);
    }
  }
  const QColor clear(8, 10, 16);
  cb->beginPass(target_.get(), clear, {1.0f, 0}, updates);
  if (star_pipeline_ != nullptr && star_buffer_ != nullptr) {
    cb->setGraphicsPipeline(star_pipeline_.get());
    cb->setViewport(
        {0.0f, 0.0f, static_cast<float>(viewport_.width), static_cast<float>(viewport_.height)});
    cb->setShaderResources(bindings_.get());
    const QRhiCommandBuffer::VertexInput star_input{star_buffer_.get(), 0};
    cb->setVertexInput(0, 1, &star_input);
    cb->draw(400);
  }
  if (!box_vertices.empty() && box_vertices_ > 0) {
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
  if (text_pipeline_ != nullptr && text_vertices_ > 0) {
    cb->setGraphicsPipeline(text_pipeline_.get());
    cb->setViewport(
        {0.0f, 0.0f, static_cast<float>(viewport_.width), static_cast<float>(viewport_.height)});
    cb->setShaderResources(text_bindings_.get());
    const QRhiCommandBuffer::VertexInput text_input{text_buffer_.get(), 0};
    cb->setVertexInput(0, 1, &text_input);
    cb->draw(text_vertices_);
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
