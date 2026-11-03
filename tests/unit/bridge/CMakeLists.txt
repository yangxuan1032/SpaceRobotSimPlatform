add_executable(test_transport test_transport.cpp)
target_link_libraries(test_transport PRIVATE srsp_kernel Catch2::Catch2WithMain)
target_include_directories(test_transport PRIVATE "${PROJECT_SOURCE_DIR}/src/bridge/include")

add_test(NAME unit.transport COMMAND test_transport)
set_tests_properties(unit.transport PROPERTIES TIMEOUT 30)

if(MSVC)
  target_compile_options(test_transport PRIVATE /W4)
else()
  target_compile_options(test_transport PRIVATE -Wall -Wextra -Wpedantic)
endif()