add_executable(test_null_renderer test_null_renderer.cpp)
target_link_libraries(test_null_renderer PRIVATE srsp_render Catch2::Catch2WithMain)

add_test(NAME unit.render COMMAND test_null_renderer)
set_tests_properties(unit.render PROPERTIES TIMEOUT 30)

add_executable(test_projection test_projection.cpp)
target_link_libraries(test_projection PRIVATE srsp_render Catch2::Catch2WithMain)

add_test(NAME unit.projection COMMAND test_projection)
set_tests_properties(unit.projection PROPERTIES TIMEOUT 30)

if(MSVC)
  target_compile_options(test_null_renderer PRIVATE /W4)
  target_compile_options(test_projection PRIVATE /W4)
else()
  target_compile_options(test_null_renderer PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(test_projection PRIVATE -Wall -Wextra -Wpedantic)
endif()
