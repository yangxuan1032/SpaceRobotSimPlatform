/**
 * @file lockfile.hpp
 * @brief Text lock file for replay: what produced a recording and may replay it.
 *
 * The lock file is the artefact a deployment ships next to a recording. It keeps
 * the reproducibility keys in a stable, line-oriented format so a replay tool can
 * refuse a recording made by a different plugin set, integrator or toolchain.
 */
#ifndef SRSP_RECORD_LOCKFILE_HPP
#define SRSP_RECORD_LOCKFILE_HPP

#include <map>
#include <string>

namespace srsp::record {

/** @brief Keys that a lock file carries. */
inline constexpr const char* kLockKeys[] = {"integrator",       "solver",   "encoding",
                                            "plugin_lock_hash", "compiler", "build_flags"};

/**
 * @brief Extracts the lock keys from a channel metadata map.
 * @param metadata Channel metadata as read from a recording.
 * @return Map containing every key that was present.
 */
[[nodiscard]] inline std::map<std::string, std::string> lock_from_metadata(
    const std::map<std::string, std::string>& metadata) {
  std::map<std::string, std::string> lock;
  for (const char* key : kLockKeys) {
    const auto it = metadata.find(key);
    if (it != metadata.end()) {
      lock[key] = it->second;
    }
  }
  return lock;
}

/**
 * @brief Serialises a lock to `key=value` lines, sorted by key.
 * @param lock Lock entries.
 */
[[nodiscard]] inline std::string serialize_lock(const std::map<std::string, std::string>& lock) {
  std::string text = "# srsp replay lock\n";
  for (const auto& [key, value] : lock) {
    text += key;
    text += '=';
    text += value;
    text += '\n';
  }
  return text;
}

/**
 * @brief Parses `key=value` lines, ignoring blanks and `#` comments.
 * @param text Lock file contents.
 */
[[nodiscard]] inline std::map<std::string, std::string> parse_lock(const std::string& text) {
  std::map<std::string, std::string> lock;
  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t end = text.find('\n', start);
    std::string line =
        text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    start = end == std::string::npos ? text.size() : end + 1;

    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    lock[line.substr(0, equals)] = line.substr(equals + 1);
  }
  return lock;
}

/**
 * @brief Compares a recording lock against the expected one.
 * @param recorded Keys read from the recording.
 * @param expected Keys from the lock file.
 * @param key Receives the first offending key.
 * @return True when every expected key is present and equal.
 */
[[nodiscard]] inline bool lock_matches(const std::map<std::string, std::string>& recorded,
                                       const std::map<std::string, std::string>& expected,
                                       std::string* key = nullptr) {
  for (const auto& [name, value] : expected) {
    const auto it = recorded.find(name);
    if (it == recorded.end() || it->second != value) {
      if (key != nullptr) {
        *key = name;
      }
      return false;
    }
  }
  return true;
}

}  // namespace srsp::record

#endif  // SRSP_RECORD_LOCKFILE_HPP