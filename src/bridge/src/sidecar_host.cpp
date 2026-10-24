/**
 * @file sidecar_host.cpp
 * @brief gRPC client implementation for the sidecar transport.
 */
#include <srsp/bridge/sidecar_host.hpp>

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <utility>

#include "srsp/v1/plugin.grpc.pb.h"

namespace srsp::bridge {
namespace {

void set_error(srsp_error* err, srsp_status code, const char* message) noexcept {
  if (err == nullptr) {
    return;
  }
  err->struct_size = sizeof(srsp_error);
  err->code = code;
  err->message = message;
  err->detail_json = nullptr;
}

srsp_status map_grpc_status(const grpc::Status& status, const char* what,
                            srsp_error* err) noexcept {
  if (status.ok()) {
    return SRSP_OK;
  }
  if (status.error_code() == grpc::StatusCode::NOT_FOUND) {
    set_error(err, SRSP_E_NOT_FOUND, what);
    return SRSP_E_NOT_FOUND;
  }
  if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ||
      status.error_code() == grpc::StatusCode::UNAVAILABLE) {
    set_error(err, SRSP_E_TIMEOUT, what);
    return SRSP_E_TIMEOUT;
  }
  if (status.error_code() == grpc::StatusCode::PERMISSION_DENIED) {
    set_error(err, SRSP_E_VERSION_MISMATCH, what);
    return SRSP_E_VERSION_MISMATCH;
  }
  set_error(err, SRSP_E_INTERNAL, what);
  return SRSP_E_INTERNAL;
}

}  // namespace

SidecarHost::SidecarHost(std::string address, std::string token, int timeout_ms)
    : address_(std::move(address)),
      token_(std::move(token)),
      timeout_ms_(timeout_ms) {}

SidecarHost::~SidecarHost() = default;

srsp_status SidecarHost::connect(srsp_error* err) {
  channel_ = grpc::CreateChannel(address_, grpc::InsecureChannelCredentials());
  auto stub = srsp::v1::PluginRuntime::NewStub(channel_);

  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::milliseconds(timeout_ms_));

  srsp::v1::HandshakeRequest request;
  request.set_token(token_);
  request.set_host_abi_major(SRSP_ABI_MAJOR);
  request.set_host_abi_minor(SRSP_ABI_MINOR);

  srsp::v1::HandshakeReply reply;
  const grpc::Status status = stub->Handshake(&context, request, &reply);
  if (!status.ok()) {
    return map_grpc_status(status, "handshake failed", err);
  }
  plugin_id_ = reply.plugin_id();
  return SRSP_OK;
}

srsp_status SidecarHost::invoke(const std::string& method,
                                const std::string& payload, std::string* out,
                                srsp_error* err) {
  if (!channel_) {
    set_error(err, SRSP_E_NOT_READY, "sidecar is not connected");
    return SRSP_E_NOT_READY;
  }
  auto stub = srsp::v1::PluginRuntime::NewStub(channel_);

  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::milliseconds(timeout_ms_));

  srsp::v1::InvokeRequest request;
  request.set_method(method);
  request.set_payload(payload);

  srsp::v1::InvokeResponse reply;
  const grpc::Status status = stub->Invoke(&context, request, &reply);
  if (!status.ok()) {
    return map_grpc_status(status, "invoke failed", err);
  }
  if (reply.has_error() && reply.error().code() != 0) {
    if (err != nullptr) {
      err->struct_size = sizeof(srsp_error);
      err->code = static_cast<srsp_status>(reply.error().code());
      err->message = nullptr;
      err->detail_json = nullptr;
    }
    return static_cast<srsp_status>(reply.error().code());
  }
  if (out != nullptr) {
    *out = reply.payload();
  }
  return SRSP_OK;
}

bool SidecarHost::ping(uint64_t step_index) {
  if (!channel_) {
    return false;
  }
  auto stub = srsp::v1::PluginRuntime::NewStub(channel_);

  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::milliseconds(timeout_ms_));

  srsp::v1::PingRequest request;
  request.set_step_index(step_index);
  srsp::v1::PingReply reply;
  const grpc::Status status = stub->Ping(&context, request, &reply);
  return status.ok() && reply.ok();
}

void SidecarHost::shutdown() {
  if (!channel_) {
    return;
  }
  auto stub = srsp::v1::PluginRuntime::NewStub(channel_);
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::milliseconds(timeout_ms_));
  srsp::v1::ShutdownRequest request;
  request.set_timeout_ms(static_cast<uint32_t>(timeout_ms_));
  srsp::v1::Empty reply;
  (void)stub->Shutdown(&context, request, &reply);
}

}  // namespace srsp::bridge
