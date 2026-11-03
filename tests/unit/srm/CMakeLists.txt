add_executable(test_srm test_srm.cpp)
target_link_libraries(test_srm PRIVATE srsp_model Catch2::Catch2WithMain)

add_test(NAME unit.srm COMMAND test_srm)
set_tests_properties(unit.srm PROPERTIES
  TIMEOUT 30
  ENVIRONMENT "SRSP_TEST_MODELS=${CMAKE_SOURCE_DIR}/examples/models")

add_executable(test_urdf test_urdf.cpp)
target_link_libraries(test_urdf PRIVATE srsp_model Catch2::Catch2WithMain)

add_test(NAME unit.urdf COMMAND test_urdf)
set_tests_properties(unit.urdf PROPERTIES TIMEOUT 30)

add_executable(test_mjcf test_mjcf.cpp)
target_link_libraries(test_mjcf PRIVATE srsp_model Catch2::Catch2WithMain)

add_test(NAME unit.mjcf COMMAND test_mjcf)
set_tests_properties(unit.mjcf PROPERTIES TIMEOUT 30)

if(MSVC)
  target_compile_options(test_srm PRIVATE /W4)
  target_compile_options(test_urdf PRIVATE /W4)
  target_compile_options(test_mjcf PRIVATE /W4)
else()
  target_compile_options(test_srm PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(test_urdf PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(test_mjcf PRIVATE -Wall -Wextra -Wpedantic)
endif()
