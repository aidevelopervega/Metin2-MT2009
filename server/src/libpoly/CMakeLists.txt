# ==============================================================================
# libpoly CMakeLists.txt
# ==============================================================================

set(POLY_SOURCES
    Base.cc
    Symbol.cc
    SymTable.cc
    Poly.cc
)

add_library(poly STATIC ${POLY_SOURCES})

target_include_directories(poly PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/server/include"
)

target_compile_options(poly PRIVATE
    -fno-exceptions
    -fno-rtti
)
