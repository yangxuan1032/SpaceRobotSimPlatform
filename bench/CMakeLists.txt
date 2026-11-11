#
# Benchmarks are executables, not tests: they print metrics and are collected by
# the nightly job. CTest entries are registered so they still run in CI and show
# up in the log without gating on machine speed.
#
add_executable(bench_dynamics bench_dynamics.cpp)
target_link_libraries(bench_dynamics PRIVATE srsp_dynamics)

add_executable(bench_record bench_record.cpp)
target_link_libraries(bench_record PRIVATE srsp_record srsp_dynamics)

add_test(NAME bench.dynamics COMMAND bench_dynamics)
set_tests_properties(bench.dynamics PROPERTIES
  TIMEOUT 300
  LABELS "bench"
  PASS_REGULAR_EXPRESSION "budget_us=300 verdict=")

add_test(NAME bench.record COMMAND bench_record)
set_tests_properties(bench.record PROPERTIES
  TIMEOUT 300
  LABELS "bench"
  ENVIRONMENT "SRSP_TEST_MODELS=${CMAKE_SOURCE_DIR}/examples/models"
  PASS_REGULAR_EXPRESSION "hash_mismatches=0")

if(MSVC)
  target_compile_options(bench_dynamics PRIVATE /W4)
  target_compile_options(bench_record PRIVATE /W4)
else()
  target_compile_options(bench_dynamics PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(bench_record PRIVATE -Wall -Wextra -Wpedantic)
endif()
# GPU frame-time benchmark (only when the RHI backend is available).
if(TARGET srsp_render_rhi)
  add_executable(bench_rhi bench_rhi.cpp)
  target_link_libraries(bench_rhi PRIVATE srsp_render_rhi)
  if(MSVC)
    target_compile_options(bench_rhi PRIVATE /W4)
  else()
    target_compile_options(bench_rhi PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()
# Plugin call overhead, referenced against the 20 us per-call budget.
add_executable(bench_plugin_call bench_plugin_call.cpp)
target_link_libraries(bench_plugin_call PRIVATE srsp_core)
add_test(NAME bench.plugin_call COMMAND bench_plugin_call)
set_tests_properties(bench.plugin_call PROPERTIES
  TIMEOUT 300
  LABELS "bench"
  ENVIRONMENT "SRSP_HELLO_PLUGIN=$<TARGET_FILE:srsp_hello_plugin>"
  PASS_REGULAR_EXPRESSION "plugin_budget_us=20 verdict=")
if(MSVC)
  target_compile_options(bench_plugin_call PRIVATE /W4)
else()
  target_compile_options(bench_plugin_call PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(bench_world bench_world.cpp)
target_link_libraries(bench_world PRIVATE srsp_kernel)
add_test(NAME bench.world COMMAND bench_world)
set_tests_properties(bench.world PROPERTIES TIMEOUT 120)