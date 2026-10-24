/**
 * @file sidecar_server.cpp
 * @brief gRPC server that exposes an in-process plugin over the sidecar
 *        transport.
 */
#include <srsp/bridge/sidecar_server.hpp>

#include <srsp/core/plugin_host.hpp>

#include <grpcpp/grpcpp.h>

#include <cstring>
#include <string>

#include "srsp/v1/plugin.grpc.pb.h"

#if defined(_WIN32)
#  include <malloc.h>
#else
#  include <cstdlib>
#endif

namespace srsp::bridge {
namespace {

srsp_status SRSP_CALL srv_alloc(void*, uint64_t size, uint64_t alignment,
                                void** out) {
  if (out == nullptr || size == 0) {
    return SRSP_E_INVALID_ARGUMENT;
  }
  if (alignment < alignof(void*)) {
    alignment = alignof(void*);
  }
#if defined(_WIN32)
  *out = _aligned_malloc(static_cast<size_t>(size), static_cast<size_t>(alignment));
#else
  if (posix_memalign(out, static_cast<size_t>(alignment),
                     static_cast<size_t>(size)) != 0) {
    *out = nullptr;
  }
#endif
  return *out != nullptr ? SRSP_OK : SRSP_E_OUT_OF_MEMORY;
}

void SRSP_CALL srv_free(void*, void* ptr) {
#if defined(_WIN32)
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

void SRSP_CALL srv_log(void*, int32_t, srsp_str) {}

double SRSP_CALL srv_time(void*) { return 0.0; }

int64_t SRSP_CALL srv_time_ns(void*) { return 0; }

srsp_status SRSP_CALL srv_publish(void*, srsp_str, srsp_bytes, uint64_t) {
  return SRSP_E_UNSUPPORTED;
}

srsp_status SRSP_CALL srv_shm_map(void*, srsp_str, uint64_t, int32_t, void**) {
  return SRSP_E_UNSUPPORTED;
}

void SRSP_CALL srv_shm_unmap(void*, void*, uint64_t) {}

srsp_host_v1 make_services() {
  srsp_host_v1 services{};
  services.struct_size = sizeof(srsp_host_v1);
  services.log = srv_log;
  services.time_tai = srv_time;
  services.time_tai_ns = srv_time_ns;
  services.alloc = srv_alloc;
  services.free = srv_free;
  services.publish = srv_publish;
  services.shm_map = srv_shm_map;
  services.shm_unmap = srv_shm_unmap;
  return services;
}

srsp_str to_str(const std::string& text) {
  srsp_str result{};
  result.data = text.data();
  result.size = text.size();
  return result;
}

/** @brief PluginRuntime implementation backed by one loaded plugin. */
class Runtime final : public srsp::v1::PluginRuntime::Service {
 public:
  Runtime(std::string token, std::string plugin_path)
      : token_(std::move(token)),
        plugin_path_(std::move(plugin_path)),
        host_(make_services()) {}

  ~Runtime() override { host_.unload(); }

  bool load() {
    return host_.load(plugin_path_, nullptr) == SRSP_OK &&
           host_.init(srsp_str{}, nullptr) == SRSP_OK;
  }

  grpc::Status Handshake(grpc::ServerContext*, const srsp::v1::HandshakeRequest* request,
                         srsp::v1::HandshakeReply* reply) override {
    if (request->token() != token_) {
      return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "bad token");
    }
    if (request->host_abi_major() != SRSP_ABI_MAJOR) {
      return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                          "unsupported host ABI");
    }
    reply->set_plugin_id("org.srsp.sidecar.hello");
    reply->set_api("1.0");
    reply->set_transport("grpc");
    return grpc::Status::OK;
  }

  grpc::Status Invoke(grpc::ServerContext*, const srsp::v1::InvokeRequest* request,
                      srsp::v1::InvokeResponse* reply) override {
    const std::string payload = request->payload();
    srsp_buffer response{};
    srsp_error err{};
    const srsp_status status = host_.call(to_str(request->method()),
                                          srsp_bytes{}, &response, &err);
    if (status != SRSP_OK) {
      auto* error = reply->mutable_error();
      error->set_code(static_cast<int32_t>(status));
      error->set_message(err.message != nullptr ? err.message : "call failed");
      return grpc::Status::OK;
    }
    if (response.data != nullptr && response.size > 0) {
      reply->set_payload(reinterpret_cast<const char*>(response.data),
                         static_cast<size_t>(response.size));
    }
    srsp_host_v1 services = make_services();
    services.free(services.user, response.data);
    return grpc::Status::OK;
  }

  grpc::Status Ping(grpc::ServerContext*, const srsp::v1::PingRequest* request,
                    srsp::v1::PingReply* reply) override {
    reply->set_ok(host_.loaded() && host_.initialized());
    reply->set_t_tai(0.0);
    reply->set_step_index(request->step_index());
    return grpc::Status::OK;
  }

  grpc::Status Checkpoint(grpc::ServerContext*, const srsp::v1::CheckpointRequest*,
                          srsp::v1::BlobReply* reply) override {
    srsp_buffer out{};
    if (host_.checkpoint(&out, nullptr) != SRSP_OK) {
      return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "checkpoint failed");
    }
    if (out.data != nullptr && out.size > 0) {
      reply->set_inline_data(reinterpret_cast<const char*>(out.data),
                            static_cast<size_t>(out.size));
    }
    srsp_host_v1 services = make_services();
    services.free(services.user, out.data);
    return grpc::Status::OK;
  }

  grpc::Status Restore(grpc::ServerContext*, const srsp::v1::RestoreRequest* request,
                       srsp::v1::Empty*) override {
    const std::string& data = request->inline_data();
    const srsp_bytes payload{
        reinterpret_cast<const uint8_t*>(data.data()), data.size()};
    if (host_.restore(payload, nullptr) != SRSP_OK) {
      return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "restore failed");
    }
    return grpc::Status::OK;
  }

  grpc::Status Shutdown(grpc::ServerContext*, const srsp::v1::ShutdownRequest*,
                        srsp::v1::Empty*) override {
    host_.unload();
    return grpc::Status::OK;
  }

  grpc::Status AttachShm(grpc::ServerContext*, const srsp::v1::AttachShmRequest*,
                         srsp::v1::Empty*) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                        "shared memory is reserved for M2");
  }

 private:
  std::string token_;
  std::string plugin_path_;
  srsp::core::PluginHost host_;
};

}  // namespace

struct SidecarServer::Impl {
  std::unique_ptr<Runtime> runtime;
  std::unique_ptr<grpc::Server> server;
};

SidecarServer::SidecarServer() : impl_(std::make_unique<Impl>()) {}
SidecarServer::~SidecarServer() { stop(); }

bool SidecarServer::start(const std::string& address, const std::string& token,
                          const std::string& plugin_path) {
  stop();
  impl_->runtime = std::make_unique<Runtime>(token, plugin_path);
  if (!impl_->runtime->load()) {
    impl_->runtime.reset();
    return false;
  }

  grpc::ServerBuilder builder;
  int selected_port = 0;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials(),
                           &selected_port);
  builder.RegisterService(impl_->runtime.get());
  impl_->server = builder.BuildAndStart();
  if (impl_->server == nullptr || selected_port == 0) {
    stop();
    return false;
  }
  port_ = selected_port;
  address_ = "127.0.0.1:" + std::to_string(selected_port);
  return true;
}

void SidecarServer::stop() {
  if (impl_ && impl_->server) {
    impl_->server->Shutdown();
    impl_->server.reset();
  }
  if (impl_ && impl_->runtime) {
    impl_->runtime.reset();
  }
  port_ = 0;
  address_.clear();
}

}  // namespace srsp::bridge
