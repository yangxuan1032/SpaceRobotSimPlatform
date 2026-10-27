/**
 * @file unsupported_capability.cpp
 * @brief Contract test for the error-code table and the "unsupported capability"
 *        rule documented in docs/02 section 15.
 *
 * The runtime side of that rule (a manifest asking for `srsp.dynamics/batch@1`
 * must fail with SRSP_E_UNSUPPORTED) becomes testable once the batch capability
 * exists; this test pins the part that is already contractual: the codes are
 * stable, unique, non-zero and the unsupported code keeps its documented value.
 */
#include <srsp/plugin_api.h>

#include <vector>

#include <cstdio>

namespace {

struct Code {
  const char* name;
  int value;
};

const Code kCodes[] = {
    {"SRSP_OK", SRSP_OK},
    {"SRSP_E_INVALID_ARGUMENT", SRSP_E_INVALID_ARGUMENT},
    {"SRSP_E_UNSUPPORTED", SRSP_E_UNSUPPORTED},
    {"SRSP_E_NOT_READY", SRSP_E_NOT_READY},
    {"SRSP_E_INVALID_STATE", SRSP_E_INVALID_STATE},
    {"SRSP_E_NOT_FOUND", SRSP_E_NOT_FOUND},
    {"SRSP_E_TIMEOUT", SRSP_E_TIMEOUT},
    {"SRSP_E_OUT_OF_MEMORY", SRSP_E_OUT_OF_MEMORY},
    {"SRSP_E_BUFFER_TOO_SMALL", SRSP_E_BUFFER_TOO_SMALL},
    {"SRSP_E_VERSION_MISMATCH", SRSP_E_VERSION_MISMATCH},
    {"SRSP_E_CANCELLED", SRSP_E_CANCELLED},
    {"SRSP_E_INTERNAL", SRSP_E_INTERNAL},
};

int fail(const char* what) {
  std::fprintf(stderr, "FAIL: %s\n", what);
  return 1;
}

}  // namespace

// The documented value must never drift: plugins compare against it directly.
static_assert(SRSP_OK == 0, "SRSP_OK must stay 0");
static_assert(SRSP_E_UNSUPPORTED == 2, "SRSP_E_UNSUPPORTED must stay 2");

int main() {
  const std::size_t count = sizeof(kCodes) / sizeof(kCodes[0]);
  if (SRSP_E_UNSUPPORTED == SRSP_OK) {
    return fail("unsupported code must differ from SRSP_OK");
  }

  for (std::size_t i = 0; i < count; ++i) {
    if (kCodes[i].value == 0 && kCodes[i].value != SRSP_OK) {
      return fail("non-OK code must not be zero");
    }
    for (std::size_t j = i + 1; j < count; ++j) {
      if (kCodes[i].value == kCodes[j].value) {
        std::fprintf(stderr, "FAIL: duplicate value %d (%s / %s)\n", kCodes[i].value,
                     kCodes[i].name, kCodes[j].name);
        return 1;
      }
    }
  }

  // Error codes are positive so that a C caller can map them to an enum without
  // sign ambiguity; a negative value would collide with errno-style APIs.
  for (std::size_t i = 0; i < count; ++i) {
    if (kCodes[i].value < 0) {
      std::fprintf(stderr, "FAIL: negative code %s\n", kCodes[i].name);
      return 1;
    }
  }

  std::printf("PASS: error codes stable (%zu codes, unsupported=%d)\n", count, SRSP_E_UNSUPPORTED);
  return 0;
}
