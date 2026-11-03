add_executable(test_snapshot test_snapshot.cpp)
target_link_libraries(test_snapshot PRIVATE srsp_kernel Catch2::Catch2WithMain)

add_test(NAME unit.snapshot COMMAND test_snapshot)
set_tests_properties(unit.snapshot PROPERTIES TIMEOUT 30)

if(MSVC)
  target_compile_options(test_snapshot PRIVATE /W4)
else()
  target_compile_options(test_snapshot PRIVATE -Wall -Wextra -Wpedantic)
endif()