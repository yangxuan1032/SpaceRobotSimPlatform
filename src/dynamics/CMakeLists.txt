#
# srsp_dynamics: floating-base rigid-body dynamics (mass matrix, gravity,
# Jacobians, momentum, integrator). Eigen provides the linear algebra.
#
find_package(Eigen3 CONFIG REQUIRED)

add_library(srsp_dynamics STATIC
  src/force_models.cpp
  src/rigid_body.cpp)
target_include_directories(srsp_dynamics PUBLIC include)
target_link_libraries(srsp_dynamics PUBLIC srsp_model Eigen3::Eigen)
set_target_properties(srsp_dynamics PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
  target_compile_options(srsp_dynamics PRIVATE /W4)
else()
  target_compile_options(srsp_dynamics PRIVATE -Wall -Wextra -Wpedantic)
endif()
