set(TARGET libngh)
file(GLOB_RECURSE SRC "ngh/*.h" "ngh/*.cc")
add_library(${TARGET} ${SRC})
target_link_libraries(${TARGET} PUBLIC
  ${Boost_LIBRARIES}
  simdjson
)
target_common(${TARGET})
