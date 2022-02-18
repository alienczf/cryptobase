# Don't expect any linking to this shared library
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

# Add library to gh install
target_module(${TARGET})