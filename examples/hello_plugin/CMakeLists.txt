add_library(srsp_hello_plugin MODULE plugin.cpp)
target_link_libraries(srsp_hello_plugin PRIVATE SRSP::api)

# Plugins must be self-contained so a host built with a different toolchain
# can load them (see docs/02-plugin-contract.md and 06-packaging.md):
# static-link the C/C++ runtime on every toolchain.
if(MINGW)
  target_link_options(srsp_hello_plugin PRIVATE -static-libgcc -static-libstdc++)
elseif(MSVC)
  set_property(TARGET srsp_hello_plugin PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set_target_properties(srsp_hello_plugin PROPERTIES
  PREFIX ""
  OUTPUT_NAME "srsp_hello"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/org.srsp.examples.hello/bin"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/org.srsp.examples.hello/bin")

add_custom_command(TARGET srsp_hello_plugin POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
          "${CMAKE_BINARY_DIR}/plugins/org.srsp.examples.hello/plugin.json"
  COMMENT "Copying plugin manifest")

add_executable(srsp_hello_host host_demo.cpp)
target_link_libraries(srsp_hello_host PRIVATE SRSP::api ${CMAKE_DL_LIBS})

add_test(NAME hello_plugin_abi
         COMMAND srsp_hello_host $<TARGET_FILE:srsp_hello_plugin>)
set_tests_properties(hello_plugin_abi PROPERTIES TIMEOUT 30)

if(MSVC)
  target_compile_options(srsp_hello_plugin PRIVATE /W4)
  target_compile_options(srsp_hello_host PRIVATE /W4)
else()
  target_compile_options(srsp_hello_plugin PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(srsp_hello_host PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SRSP_WARNINGS_AS_ERRORS)
  target_compile_options(srsp_hello_plugin PRIVATE -Werror)
  target_compile_options(srsp_hello_host PRIVATE -Werror)
endif()
