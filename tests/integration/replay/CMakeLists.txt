# Deterministic record/replay check: records a trajectory to MCAP, reads it
# back and re-simulates, comparing per-step XXH3-128 state hashes. Standalone
# executable so it does not depend on the unit-test framework ordering.
if(TARGET srsp_record AND TARGET srsp_dynamics)
  add_executable(test_replay test_replay.cpp)
  target_link_libraries(test_replay PRIVATE srsp_record srsp_dynamics)

  add_test(NAME integration.replay COMMAND test_replay)
  set_tests_properties(integration.replay PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "SRSP_TEST_MODELS=${CMAKE_SOURCE_DIR}/examples/models"
    PASS_REGULAR_EXPRESSION "PASS: 200 steps recorded")

  if(MSVC)
    target_compile_options(test_replay PRIVATE /W4)
  else()
    target_compile_options(test_replay PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()
