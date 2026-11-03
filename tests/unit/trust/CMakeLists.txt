add_executable(test_trust test_trust.cpp)
target_link_libraries(test_trust PRIVATE srsp_kernel Catch2::Catch2WithMain)

add_test(NAME unit.trust COMMAND test_trust)
set_tests_properties(unit.trust PROPERTIES TIMEOUT 30)

if(MSVC)
  target_compile_options(test_trust PRIVATE /W4)
else()
  target_compile_options(test_trust PRIVATE -Wall -Wextra -Wpedantic)
endif()