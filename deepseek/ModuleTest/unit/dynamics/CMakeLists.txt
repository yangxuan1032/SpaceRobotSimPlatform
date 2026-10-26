add_executable(test_rigid_body test_rigid_body.cpp)
target_link_libraries(test_rigid_body PRIVATE srsp_dynamics Catch2::Catch2WithMain)

add_test(NAME unit.dynamics COMMAND test_rigid_body)
set_tests_properties(unit.dynamics PROPERTIES
  TIMEOUT 120
  ENVIRONMENT "SRSP_TEST_MODELS=${CMAKE_SOURCE_DIR}/examples/models")

add_executable(test_force_models test_force_models.cpp)
target_link_libraries(test_force_models PRIVATE srsp_dynamics Catch2::Catch2WithMain)

add_test(NAME unit.force_models COMMAND test_force_models)
set_tests_properties(unit.force_models PROPERTIES TIMEOUT 30)

if(MSVC)
  target_compile_options(test_rigid_body PRIVATE /W4)
  target_compile_options(test_force_models PRIVATE /W4)
else()
  target_compile_options(test_rigid_body PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(test_force_models PRIVATE -Wall -Wextra -Wpedantic)
endif()
