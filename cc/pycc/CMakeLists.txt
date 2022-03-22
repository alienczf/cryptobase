set(TARGET pycc)

file(GLOB_RECURSE SRC "*.h" "*.cc")

pybind11_add_module(${TARGET} ${SRC})

target_link_libraries(${TARGET} PRIVATE
  libngh
  ${VTUNE}
)

target_compile_options(${TARGET} PRIVATE
    -Wno-type-limits
)

target_module(${TARGET})
