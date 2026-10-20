// Minimal SRSP in-process plugin used as a conformance/demo fixture.
//
// It exposes one capability (`srsp.control/controller@1`) and implements the
// reserved methods `core.describe`, `core.ping` plus `controller.update`.
// All payloads are JSON for readability; real plugins use protobuf.

#include <srsp/plugin_api.h>

#include <cstring>
#include <string>

namespace {

struct HelloPlugin {
  const srsp_host_v1* host = nullptr;
  double gain = 1.0;
  uint64_t steps = 0;
};

void set_error(srsp_error* err, srsp_status code, const char* message) {
  if (err == nullptr) {
    return;
  }
  err->code = code;
  err->message = message;
  err->detail_json = nullptr;
}

srsp_status reply(HelloPlugin* self, const std::string& json, srsp_buffer* out) {
  if (out == nullptr) {
    return SRSP_E_INVALID_ARGUMENT;
  }
  out->data = nullptr;
  out->size = 0;

  void* memory = nullptr;
  const srsp_host_v1* host = self->host;
  if (host == nullptr || host->alloc == nullptr) {
    return SRSP_E_NOT_READY;
  }
  const srsp_status status =
      host->alloc(host->user, static_cast<uint64_t>(json.size()), 8, &memory);
  if (status != SRSP_OK || memory == nullptr) {
    return SRSP_E_OUT_OF_MEMORY;
  }
  std::memcpy(memory, json.data(), json.size());
  out->data = static_cast<uint8_t*>(memory);
  out->size = static_cast<uint64_t>(json.size());
  return SRSP_OK;
}

srsp_status SRSP_CALL plugin_init(void* self_ptr, const srsp_host_v1* host,
                                  srsp_str /*config_json*/, srsp_error* err) {
  auto* self = static_cast<HelloPlugin*>(self_ptr);
  if (self == nullptr || host == nullptr || host->alloc == nullptr ||
      host->free == nullptr) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "host services are incomplete");
    return SRSP_E_INVALID_ARGUMENT;
  }
  self->host = host;
  if (host->log != nullptr) {
    host->log(host->user, SRSP_LOG_INFO, SRSP_STR_LITERAL("hello plugin initialised"));
  }
  return SRSP_OK;
}

srsp_status SRSP_CALL plugin_call(void* self_ptr, srsp_str method,
                                  srsp_bytes /*request*/, srsp_buffer* response,
                                  srsp_error* err) {
  auto* self = static_cast<HelloPlugin*>(self_ptr);
  if (self == nullptr) {
    set_error(err, SRSP_E_INVALID_STATE, "plugin instance is null");
    return SRSP_E_INVALID_STATE;
  }

  const std::string name(method.data, static_cast<size_t>(method.size));

  if (name == "core.describe") {
    return reply(self,
                 R"({"id":"org.srsp.examples.hello","version":"0.1.0",)"
                 R"("capabilities":[{"name":"srsp.control/controller","version":"1.0","alias":"controller"}],)"
                 R"("methods":["core.describe","core.ping","controller.update"]})",
                 response);
  }
  if (name == "core.ping") {
    return reply(self, R"({"ok":true,"steps":)" + std::to_string(self->steps) + "}",
                 response);
  }
  if (name == "controller.update") {
    ++self->steps;
    return reply(self,
                 R"({"commands":[],"steps":)" + std::to_string(self->steps) + "}",
                 response);
  }

  set_error(err, SRSP_E_UNSUPPORTED, "unknown method");
  return SRSP_E_UNSUPPORTED;
}

srsp_status SRSP_CALL plugin_checkpoint(void* self_ptr, srsp_buffer* out,
                                        srsp_error* err) {
  auto* self = static_cast<HelloPlugin*>(self_ptr);
  if (self == nullptr || self->host == nullptr || out == nullptr) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "invalid checkpoint arguments");
    return SRSP_E_INVALID_ARGUMENT;
  }
  out->data = nullptr;
  out->size = 0;

  void* memory = nullptr;
  const srsp_status status = self->host->alloc(self->host->user, 16, 8, &memory);
  if (status != SRSP_OK || memory == nullptr) {
    return SRSP_E_OUT_OF_MEMORY;
  }
  std::memcpy(memory, &self->steps, 8);
  std::memcpy(static_cast<uint8_t*>(memory) + 8, &self->gain, 8);
  out->data = static_cast<uint8_t*>(memory);
  out->size = 16;
  return SRSP_OK;
}

srsp_status SRSP_CALL plugin_restore(void* self_ptr, srsp_bytes in, srsp_error* err) {
  auto* self = static_cast<HelloPlugin*>(self_ptr);
  if (self == nullptr || in.data == nullptr || in.size != 16) {
    set_error(err, SRSP_E_INVALID_ARGUMENT, "checkpoint payload must be 16 bytes");
    return SRSP_E_INVALID_ARGUMENT;
  }
  std::memcpy(&self->steps, in.data, 8);
  std::memcpy(&self->gain, in.data + 8, 8);
  return SRSP_OK;
}

void SRSP_CALL plugin_shutdown(void* self_ptr) {
  auto* self = static_cast<HelloPlugin*>(self_ptr);
  if (self != nullptr && self->host != nullptr && self->host->log != nullptr) {
    self->host->log(self->host->user, SRSP_LOG_INFO,
                    SRSP_STR_LITERAL("hello plugin shut down"));
  }
}

HelloPlugin& instance() {
  static HelloPlugin plugin;
  return plugin;
}

}  // namespace

extern "C" SRSP_EXPORT const srsp_plugin_v1* SRSP_CALL srsp_plugin_query(
    uint32_t host_abi_major, const srsp_host_v1* /*host*/) {
  if (host_abi_major != SRSP_ABI_MAJOR) {
    return nullptr;
  }
  static const srsp_plugin_v1 api = {
      sizeof(srsp_plugin_v1), "org.srsp.examples.hello", "0.1.0", &instance(),
      plugin_init, plugin_call, plugin_checkpoint, plugin_restore, plugin_shutdown};
  return &api;
}
