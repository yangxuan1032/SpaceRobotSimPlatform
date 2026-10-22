add_executable(srsp_abi_layout layout_static_assert.cpp)
target_link_libraries(srsp_abi_layout PRIVATE SRSP::api)

add_test(NAME abi_layout COMMAND srsp_abi_layout)
set_tests_properties(abi_layout PROPERTIES TIMEOUT 30)

if(MSVC)
  target_compile_options(srsp_abi_layout PRIVATE /W4)
else()
  target_compile_options(srsp_abi_layout PRIVATE -Wall -Wextra -Wpedantic)
endif()
