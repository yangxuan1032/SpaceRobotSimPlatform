/**
 * @file command_line.hpp
 * @brief Small helpers for reading CLI positionals without raw index math.
 *
 * The CLI once used `args[2]` directly and printed `args[1]` (the command) in
 * error messages, so a missing recording produced "cannot read recording
 * 'lock'". Every command now goes through these helpers, which keep the index
 * in one place and print the value that was actually used.
 */
#ifndef SRSP_CLI_COMMAND_LINE_HPP
#define SRSP_CLI_COMMAND_LINE_HPP

#include <string>
#include <vector>

#include <cstdio>

namespace srsp::cli {

/**
 * @brief Returns the i-th positional argument after the command, or nullptr.
 * @param args Full argv vector (index 0 is the program, 1 the command).
 * @param index Zero-based positional index, so 0 means `args[2]`.
 */
[[nodiscard]] inline const std::string* positional(const std::vector<std::string>& args,
                                                   std::size_t index) noexcept {
  const std::size_t position = index + 2u;
  return args.size() > position ? &args[position] : nullptr;
}

/**
 * @brief Fetches a required positional or reports why it is missing.
 * @return The value, or an empty optional-like result via @p ok.
 */
[[nodiscard]] inline bool require_positional(const std::vector<std::string>& args,
                                             std::size_t index, const char* command,
                                             const char* what, const std::string** out) noexcept {
  const std::string* value = positional(args, index);
  if (value == nullptr) {
    std::fprintf(stderr, "error: %s needs %s\n", command, what);
    return false;
  }
  *out = value;
  return true;
}

}  // namespace srsp::cli

#endif  // SRSP_CLI_COMMAND_LINE_HPP
