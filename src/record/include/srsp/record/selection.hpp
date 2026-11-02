/**
 * @file selection.hpp
 * @brief Records which solver, integrator and encoding produced a stream.
 *
 * A recording is only reproducible when the reader knows how the numbers were
 * produced. These keys are written into the channel metadata next to the schema
 * hash, following the append-only convention of docs/02 section 17.1.
 */
#ifndef SRSP_RECORD_SELECTION_HPP
#define SRSP_RECORD_SELECTION_HPP

#include <cstdint>
#include <map>
#include <string>

namespace srsp::record {

/** @brief Time integrator used while recording. */
enum class IntegratorTag : std::uint32_t {
  kRk4 = 0,           ///< Classical RK4 (default).
  kSemiImplicit = 1,  ///< Symplectic Euler, first order.
};

/** @brief Dynamics solver used while recording. */
enum class SolverTag : std::uint32_t {
  kComposite = 0,    ///< CRBA mass matrix plus a dense solve (default).
  kArticulated = 1,  ///< O(n) articulated-body solver (reserved).
};

/** @brief Message encoding of a channel. */
enum class EncodingTag : std::uint32_t {
  kRawF64 = 0,    ///< Raw little-endian doubles (1.x default).
  kArrowIpc = 1,  ///< Arrow IPC record batches (reserved for 2.0).
};

/** @brief Stable name for diagnostics and metadata. */
[[nodiscard]] inline const char* to_string(IntegratorTag tag) noexcept {
  switch (tag) {
    case IntegratorTag::kRk4:
      return "rk4";
    case IntegratorTag::kSemiImplicit:
      return "semi_implicit";
  }
  return "unknown";
}

/** @brief Stable name for diagnostics and metadata. */
[[nodiscard]] inline const char* to_string(SolverTag tag) noexcept {
  switch (tag) {
    case SolverTag::kComposite:
      return "composite";
    case SolverTag::kArticulated:
      return "articulated";
  }
  return "unknown";
}

/** @brief Stable name for diagnostics and metadata. */
[[nodiscard]] inline const char* to_string(EncodingTag tag) noexcept {
  switch (tag) {
    case EncodingTag::kRawF64:
      return "raw-f64";
    case EncodingTag::kArrowIpc:
      return "arrow";
  }
  return "unknown";
}

/**
 * @brief Builds the metadata map written on a recorded channel.
 * @param integrator Integrator used to produce the samples.
 * @param solver Solver used to produce the samples.
 * @param encoding Message encoding of the channel.
 */
[[nodiscard]] inline std::map<std::string, std::string> selection_metadata(IntegratorTag integrator,
                                                                           SolverTag solver,
                                                                           EncodingTag encoding) {
  return {{"integrator", to_string(integrator)},
          {"solver", to_string(solver)},
          {"encoding", to_string(encoding)}};
}

}  // namespace srsp::record

#endif  // SRSP_RECORD_SELECTION_HPP
