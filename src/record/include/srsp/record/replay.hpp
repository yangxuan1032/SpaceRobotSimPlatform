/**
 * @file replay.hpp
 * @brief Replay admission: can this recording be replayed by the current run?
 *
 * Two questions are answered here, both from channel metadata:
 *  - is the recording self-consistent (has the selection tags, uses a known
 *    encoding)?
 *  - does it match the run that wants to replay it (integrator/solver/lock)?
 *
 * A mismatch must never be silent: strict callers reject, read-only callers may
 * still inspect the file but must not take part in regression comparisons.
 */
#ifndef SRSP_RECORD_REPLAY_HPP
#define SRSP_RECORD_REPLAY_HPP

#include <srsp/record/mcap.hpp>
#include <srsp/record/run_metadata.hpp>

#include <map>
#include <string>
#include <vector>

namespace srsp::record {

/** @brief How strict the caller wants the check to be. */
enum class ReplayPolicy {
  kStrict = 0,         ///< Reject on any missing or mismatched key.
  kAllowReadOnly = 1,  ///< Mismatch downgrades the session to read-only.
};

/** @brief Outcome of a replay admission check. */
struct ReplayDecision {
  bool allowed = false;    ///< False means the recording cannot be opened at all.
  bool read_only = false;  ///< True when allowed but excluded from regressions.
  std::string reason;      ///< Empty on success, otherwise a user-facing message.
};

/** @brief Encoding values the reader understands. */
[[nodiscard]] inline bool known_encoding(const std::string& encoding) {
  return encoding == "raw-f64" || encoding == "arrow" || encoding == "";
}

/**
 * @brief Checks that a recording carries everything replay needs.
 * @param channels Channels as read from the file.
 * @return Decision; `allowed == false` for an unknown encoding or missing tags.
 */
[[nodiscard]] inline ReplayDecision verify_recording(const std::vector<ChannelInfo>& channels) {
  ReplayDecision decision;
  if (channels.empty()) {
    decision.reason = "recording has no channels";
    return decision;
  }
  for (const auto& channel : channels) {
    if (!known_encoding(channel.message_encoding)) {
      decision.reason = "channel '" + channel.topic + "' uses unknown encoding '" +
                        channel.message_encoding + "'";
      return decision;
    }
    for (const char* key : {"integrator", "solver", "encoding"}) {
      if (channel.metadata.find(key) == channel.metadata.end()) {
        decision.reason =
            "channel '" + channel.topic + "' is missing metadata key '" + std::string(key) + "'";
        return decision;
      }
    }
  }
  decision.allowed = true;
  return decision;
}

/**
 * @brief Checks a recording against the current run's metadata.
 * @param channels Channels as read from the file.
 * @param current Metadata of the run that would replay it.
 * @param policy Strict or read-only fallback.
 */
[[nodiscard]] inline ReplayDecision verify_against(const std::vector<ChannelInfo>& channels,
                                                   const RunMetadata& current,
                                                   ReplayPolicy policy) {
  ReplayDecision decision = verify_recording(channels);
  if (!decision.allowed) {
    return decision;
  }
  for (const auto& channel : channels) {
    std::string key;
    const SelectionMatch match = check_selection(channel.metadata, current, &key);
    if (match == SelectionMatch::kOk) {
      continue;
    }
    const char* kind = match == SelectionMatch::kMissing ? "missing" : "mismatched";
    decision.reason = "channel '" + channel.topic + "' has " + kind + " metadata key '" + key +
                      "' (recorded '" +
                      (channel.metadata.count(key) != 0u ? channel.metadata.at(key) : "<none>") +
                      "', current '" + run_metadata_map(current).at(key) + "')";
    if (policy == ReplayPolicy::kStrict) {
      decision.allowed = false;
      return decision;
    }
    decision.allowed = true;
    decision.read_only = true;
    return decision;
  }
  decision.allowed = true;
  return decision;
}

}  // namespace srsp::record

#endif  // SRSP_RECORD_REPLAY_HPP
