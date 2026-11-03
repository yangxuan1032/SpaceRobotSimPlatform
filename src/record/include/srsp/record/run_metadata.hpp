/**
 * @file run_metadata.hpp
 * @brief Reproducibility metadata for a recording: selection tags and plugin lock.
 *
 * A recording without the plugin lock and toolchain identity cannot be
 * reproduced: two machines with different plugin builds produce different
 * numbers for the same scenario. The lock is a canonical, sorted list of
 * (id, version, schema hash, api major) whose XXH3-128 digest is written next
 * to the integrator/solver/encoding tags.
 */
#ifndef SRSP_RECORD_RUN_METADATA_HPP
#define SRSP_RECORD_RUN_METADATA_HPP

#include <srsp/record/selection.hpp>
#include <srsp/record/state_hash.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace srsp::record {

/** @brief One resolved plugin pinned by a lock. */
struct PluginLockEntry {
  std::string id;
  std::string version;
  std::string schema_hash;
  std::uint32_t api_major = 1;
};

/**
 * @brief Canonical text form of a plugin lock.
 *
 * Sorted by id so two runs with the same set produce identical bytes regardless
 * of discovery order; one entry per line as `id@version#schema#api`.
 */
[[nodiscard]] inline std::string canonical_plugin_lock(std::vector<PluginLockEntry> entries) {
  std::sort(entries.begin(), entries.end(),
            [](const PluginLockEntry& a, const PluginLockEntry& b) { return a.id < b.id; });
  std::string text;
  for (const auto& entry : entries) {
    text += entry.id;
    text += '@';
    text += entry.version;
    text += '#';
    text += entry.schema_hash;
    text += '#';
    text += std::to_string(entry.api_major);
    text += '\n';
  }
  return text;
}

/** @brief Everything needed to re-run a recording with the same numbers. */
struct RunMetadata {
  IntegratorTag integrator = IntegratorTag::kRk4;
  SolverTag solver = SolverTag::kComposite;
  EncodingTag encoding = EncodingTag::kRawF64;
  std::string compiler;     ///< e.g. "GCC 16.1.0".
  std::string build_flags;  ///< e.g. "-O2 -ffp-contract=off".
  std::string plugin_lock;  ///< canonical_plugin_lock() output.
};

/** @brief Digest of the canonical lock, as lower-case hex. */
[[nodiscard]] inline std::string plugin_lock_hash(const std::string& canonical_lock) {
  return hash_bytes(canonical_lock.data(), canonical_lock.size()).to_hex();
}

/** @brief Builds the channel metadata map written for a recording. */
[[nodiscard]] inline std::map<std::string, std::string> run_metadata_map(
    const RunMetadata& metadata) {
  return {{"integrator", to_string(metadata.integrator)},
          {"solver", to_string(metadata.solver)},
          {"encoding", to_string(metadata.encoding)},
          {"compiler", metadata.compiler},
          {"build_flags", metadata.build_flags},
          {"plugin_lock_hash", plugin_lock_hash(metadata.plugin_lock)}};
}

/** @brief Result of comparing a recording against the current run. */
enum class SelectionMatch {
  kOk = 0,        ///< All keys present and equal.
  kMissing = 1,   ///< One of the keys is absent from the recording.
  kMismatch = 2,  ///< A key differs from the current run.
};

/**
 * @brief Compares recorded metadata with the current run.
 * @param recorded Metadata read from the channel.
 * @param current Metadata of the run that would replay the channel.
 * @param key Receives the first offending key when not kOk.
 */
[[nodiscard]] inline SelectionMatch check_selection(
    const std::map<std::string, std::string>& recorded, const RunMetadata& current,
    std::string* key = nullptr) {
  const auto expected = run_metadata_map(current);
  for (const auto& [name, value] : expected) {
    const auto it = recorded.find(name);
    if (it == recorded.end()) {
      if (key != nullptr) {
        *key = name;
      }
      return SelectionMatch::kMissing;
    }
    if (it->second != value) {
      if (key != nullptr) {
        *key = name;
      }
      return SelectionMatch::kMismatch;
    }
  }
  return SelectionMatch::kOk;
}

}  // namespace srsp::record

#endif  // SRSP_RECORD_RUN_METADATA_HPP
