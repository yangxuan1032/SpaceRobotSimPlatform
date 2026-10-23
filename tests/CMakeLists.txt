# Test suite root. Groups:
#   abi/         C ABI layout static asserts (toolchain-portability contract)
#   unit/        version negotiation / rejection paths (docs/02 section 6)
#   schema/      JSON Schema example validation (python + jsonschema)
#   conformance/ dual-transport consistency (added in M0, phase 3)
#   bench/       performance baselines      (added in M0, phase 2+)

add_subdirectory(abi)
add_subdirectory(unit)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import jsonschema"
    RESULT_VARIABLE SRSP_HAVE_JSONSCHEMA
    OUTPUT_QUIET ERROR_QUIET)
  if(SRSP_HAVE_JSONSCHEMA EQUAL 0)
    add_test(NAME schema_manifest_examples
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/schema/validate_manifest_examples.py)
    set_tests_properties(schema_manifest_examples PROPERTIES TIMEOUT 60)
  else()
    message(STATUS "Python3 found but 'jsonschema' is missing; schema example tests are disabled (pip install jsonschema to enable)")
  endif()
else()
  message(STATUS "Python3 not found; schema example tests are disabled")
endif()
