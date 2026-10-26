# Test suite root. Groups:
#   abi/         C ABI layout static asserts (toolchain-portability contract)
#   unit/        version negotiation / rejection paths (docs/02 section 6)
#   schema/      JSON Schema example validation (python + jsonschema)
#   docs/        JSON/YAML parsing and Markdown relative-link checks (CI mirror)
#   conformance/ dual-transport consistency lives in <model>/ModuleTest/integration
#   bench/       performance baselines      (added in M0, phase 2+)

add_subdirectory(abi)
add_subdirectory(unit)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  # Repository hygiene: JSON/YAML parse + Markdown links (needs no packages).
  add_test(NAME docs_check
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/docs/check_docs.py)
  set_tests_properties(docs_check PROPERTIES TIMEOUT 120)

  # Style gate: every C/C++ source must match .clang-format. The script skips
  # itself when no clang-format is available on the machine.
  add_test(NAME format_check
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/style/check_format.py)
  set_tests_properties(format_check PROPERTIES TIMEOUT 120)

  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import jsonschema"
    RESULT_VARIABLE SRSP_HAVE_JSONSCHEMA
    OUTPUT_QUIET ERROR_QUIET)
  if(SRSP_HAVE_JSONSCHEMA EQUAL 0)
    add_test(NAME schema_manifest_examples
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/schema/validate_manifest_examples.py)
    set_tests_properties(schema_manifest_examples PROPERTIES TIMEOUT 60)

    add_test(NAME schema_capability_examples
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/schema/validate_capability_examples.py)
    set_tests_properties(schema_capability_examples PROPERTIES TIMEOUT 60)
  else()
    message(STATUS "Python3 found but 'jsonschema' is missing; schema example tests are disabled (pip install jsonschema to enable)")
  endif()
else()
  message(STATUS "Python3 not found; docs and schema tests are disabled")
endif()
