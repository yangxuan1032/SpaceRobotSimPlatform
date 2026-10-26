#
# srsp_model: SRM 1.0 model layer (parse, validate, serialise).
# Imported targets are directory-scoped, so this directory looks the package up
# itself instead of relying on src/core's find_package call.
#
find_package(nlohmann_json CONFIG REQUIRED)
find_package(tinyxml2 CONFIG REQUIRED)

add_library(srsp_model STATIC
  src/mjcf_importer.cpp
  src/srm.cpp
  src/urdf_importer.cpp)
target_include_directories(srsp_model PUBLIC include)
target_link_libraries(srsp_model PUBLIC SRSP::api nlohmann_json::nlohmann_json)
target_link_libraries(srsp_model PRIVATE tinyxml2::tinyxml2)
set_target_properties(srsp_model PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
  target_compile_options(srsp_model PRIVATE /W4)
else()
  target_compile_options(srsp_model PRIVATE -Wall -Wextra -Wpedantic)
endif()
