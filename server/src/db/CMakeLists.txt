# ==============================================================================
# db Server CMakeLists.txt
# ==============================================================================

file(GLOB DB_ALL_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
set(DB_CORE_SOURCES ${DB_ALL_SOURCES})
list(FILTER DB_CORE_SOURCES EXCLUDE REGEX ".*Main\\.cpp$")

# DB cekirdek fonksiyonlari (hem db executable hem testler tarafindan paylasilir)
add_library(db_core STATIC ${DB_CORE_SOURCES})

target_include_directories(db_core PUBLIC
    ${DB_CLIENT_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../libthecore/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libsql"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libpoly"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libgame/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libgame/src"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/common/include/boost"
    "${CMAKE_SOURCE_DIR}/extern/server/include"
)

target_compile_definitions(db_core PUBLIC
    __USER__="metin2"
    __HOSTNAME__="mt2009"
    __PWD__=""
    __DB_VERSION__="${MT2_SERVER_REVISION}"
)

target_compile_options(db_core PRIVATE
    -fno-rtti
    -fno-exceptions
)

target_link_libraries(db_core PUBLIC
    thecore
    sql_lib
    poly
    game_lib
    Database::Client
)

# Gercek DB Sunucusu Binary Hedefi
add_executable(db src/Main.cpp)

target_link_libraries(db PRIVATE
    db_core
)

set_target_properties(db PROPERTIES
    OUTPUT_NAME "db_r${MT2_SERVER_REVISION}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(WIN32)
    target_link_libraries(db PRIVATE ws2_32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(db PRIVATE pthread m ssl crypto)
endif()
