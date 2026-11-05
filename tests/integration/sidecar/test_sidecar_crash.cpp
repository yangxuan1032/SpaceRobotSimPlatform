/**
 * @file test_sidecar_crash.cpp
 * @brief A sidecar that dies must not hang the host (B10).
 *
 * Starts the sidecar executable, terminates it, and checks that reading from it
 * fails within the timeout instead of blocking forever.
 *
 * Usage: test_sidecar_crash <sidecar-exe>
 */
#include <srsp/bridge/sidecar_process.hpp>

#include <string>

#include <cstdio>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: test_sidecar_crash <sidecar-exe>\n");
    return 2;
  }

  srsp::bridge::SidecarProcess process;
  if (!process.start(argv[1], {})) {
    std::fprintf(stderr, "FAIL: sidecar did not start\n");
    return 1;
  }

  process.terminate();
  if (process.running()) {
    std::fprintf(stderr, "FAIL: process still reported as running after terminate\n");
    return 1;
  }

  // A dead child must surface as a failed read, never as an indefinite block.
  std::string line;
  const bool read_ok = process.read_line(&line, 500);
  if (read_ok) {
    std::fprintf(stderr, "FAIL: read succeeded after the child terminated\n");
    return 1;
  }

  process.terminate();  // idempotent
  std::printf("PASS: dead sidecar fails fast (no hang)\n");
  return 0;
}