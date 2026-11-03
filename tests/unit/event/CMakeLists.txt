add_executable(test_event test_event.cpp)
target_link_libraries(test_event PRIVATE srsp_kernel Catch2::Catch2WithMain)

add_test(NAME unit.event COMMAND test_event)
set_tests_properties(unit.event PROPERTIES TIMEOUT 30)

if(MSVC)
  target_compile_options(test_event PRIVATE /W4)
else()
  target_compile_options(test_event PRIVATE -Wall -Wextra -Wpedantic)
endif()