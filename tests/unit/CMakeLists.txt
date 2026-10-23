add_executable(srsp_abi_negotiation abi_negotiation.cpp)
target_link_libraries(srsp_abi_negotiation PRIVATE SRSP::api ${CMAKE_DL_LIBS})

if(TARGET srsp_hello_plugin)
  add_dependencies(srsp_abi_negotiation srsp_hello_plugin)
  add_test(NAME abi_negotiation
           COMMAND srsp_abi_negotiation $<TARGET_FILE:srsp_hello_plugin>)
  set_tests_properties(abi_negotiation PROPERTIES TIMEOUT 30)
else()
  message(STATUS "srsp_hello_plugin not built; abi_negotiation test is disabled")
endif()

if(MSVC)
  target_compile_options(srsp_abi_negotiation PRIVATE /W4)
else()
  target_compile_options(srsp_abi_negotiation PRIVATE -Wall -Wextra -Wpedantic)
endif()
