#
# srsp_record: MCAP recording (flat subset) and XXH3-128 state hashing.
#
find_path(XXHASH_INCLUDE_DIR xxhash.h)
find_library(XXHASH_LIBRARY NAMES xxhash)
if(NOT XXHASH_INCLUDE_DIR OR NOT XXHASH_LIBRARY)
  message(STATUS "xxhash not found; srsp_record is disabled")
  return()
endif()

add_library(srsp_record STATIC
  src/mcap.cpp
  src/state_hash.cpp)
target_include_directories(srsp_record PUBLIC include)
target_include_directories(srsp_record PRIVATE "${XXHASH_INCLUDE_DIR}")
target_link_libraries(srsp_record PUBLIC "${XXHASH_LIBRARY}")
set_target_properties(srsp_record PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
  target_compile_options(srsp_record PRIVATE /W4)
else()
  target_compile_options(srsp_record PRIVATE -Wall -Wextra -Wpedantic)
endif()
