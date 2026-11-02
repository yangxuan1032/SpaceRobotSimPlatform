/**
 * @file transport.hpp
 * @brief Runtime choice between in-process and sidecar plugin transport.
 */
#ifndef SRSP_BRIDGE_TRANSPORT_HPP
#define SRSP_BRIDGE_TRANSPORT_HPP

#include <string_view>

namespace srsp::bridge {

/** @brief Plugin transport selection. */
enum class TransportKind {
  kInProc = 0,   ///< Load the library into this process.
  kSidecar = 1,  ///< Talk to a sidecar process over gRPC.
};

/**
 * @brief Parses a transport name.
 * @return The kind, or nullptr for an unknown name.
 */
[[nodiscard]] inline const TransportKind* parse_transport(std::string_view name) noexcept {
  static constexpr TransportKind kInProc = TransportKind::kInProc;
  static constexpr TransportKind kSidecar = TransportKind::kSidecar;
  if (name == "inproc") {
    return &kInProc;
  }
  if (name == "sidecar") {
    return &kSidecar;
  }
  return nullptr;
}

/** @brief Stable name used in diagnostics. */
[[nodiscard]] inline const char* to_string(TransportKind kind) noexcept {
  switch (kind) {
    case TransportKind::kInProc:
      return "inproc";
    case TransportKind::kSidecar:
      return "sidecar";
  }
  return "unknown";
}

}  // namespace srsp::bridge

#endif  // SRSP_BRIDGE_TRANSPORT_HPP