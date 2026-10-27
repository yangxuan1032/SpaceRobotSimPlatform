add_executable(srsp_abi_layout layout_static_assert.cpp)
target_link_libraries(srsp_abi_layout PRIVATE SRSP::api)

add_test(NAME abi_layout COMMAND srsp_abi_layout)
set_tests_properties(abi_layout PROPERTIES TIMEOUT 30)

if(MSVC)
  target_compile_options(srsp_abi_layout PRIVATE /W4)
else()
  target_compile_options(srsp_abi_layout PRIVATE -Wall -Wextra -Wpedantic)
endif()
# Contract test for the error-code table and the "unsupported capability" rule
# (docs/02 section 15). Runtime negative tests follow once batch exists.
add_executable(srsp_abi_unsupported unsupported_capability.cpp)
target_link_libraries(srsp_abi_unsupported PRIVATE SRSP::api)
add_test(NAME abi_unsupported_capability COMMAND srsp_abi_unsupported)
set_tests_properties(abi_unsupported_capability PROPERTIES TIMEOUT 30)

if(MSVC)
  target_compile_options(srsp_abi_unsupported PRIVATE /W4)
else()
  target_compile_options(srsp_abi_unsupported PRIVATE -Wall -Wextra -Wpedantic)
endif()