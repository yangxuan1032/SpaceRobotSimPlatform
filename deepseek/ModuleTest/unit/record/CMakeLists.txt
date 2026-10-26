add_executable(test_mcap test_mcap.cpp)
target_link_libraries(test_mcap PRIVATE srsp_record Catch2::Catch2WithMain)

add_test(NAME unit.mcap COMMAND test_mcap)
set_tests_properties(unit.mcap PROPERTIES TIMEOUT 60)

if(MSVC)
  target_compile_options(test_mcap PRIVATE /W4)
else()
  target_compile_options(test_mcap PRIVATE -Wall -Wextra -Wpedantic)
endif()
