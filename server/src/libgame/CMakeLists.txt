# ==============================================================================
# libgame CMakeLists.txt
# ==============================================================================

set(GAME_LIB_SOURCES
    src/grid.cc
    src/attribute.cc
    src/targa.cc
)

add_library(game_lib STATIC ${GAME_LIB_SOURCES})

target_include_directories(game_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/server/include"
)

target_link_libraries(game_lib PUBLIC thecore)

target_compile_options(game_lib PRIVATE
    -fno-exceptions
    -fno-rtti
)
