add_executable(test_blob test_blob.cpp)
target_link_libraries(test_blob PRIVATE srsp_kernel Catch2::Catch2WithMain)

add_test(NAME unit.blob COMMAND test_blob)
set_tests_properties(unit.blob PROPERTIES TIMEOUT 30)

if(MSVC)
  target_compile_options(test_blob PRIVATE /W4)
else()
  target_compile_options(test_blob PRIVATE -Wall -Wextra -Wpedantic)
endif()