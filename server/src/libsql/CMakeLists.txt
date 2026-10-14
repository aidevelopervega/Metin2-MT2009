# ==============================================================================
# libsql CMakeLists.txt
# ==============================================================================

set(SQL_LIB_SOURCES
    AsyncSQL.cpp
    Semaphore.cpp
    Statement.cpp
    Tellwait.cpp
)

add_library(sql_lib STATIC ${SQL_LIB_SOURCES})

target_include_directories(sql_lib PUBLIC
    ${DB_CLIENT_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libthecore/include"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/server/include"
)

# C++ RTTI zorunludur (game icindeki CAsyncSQL2 dinamik tip donusumu)
target_compile_options(sql_lib PRIVATE
    -fno-exceptions
)

target_link_libraries(sql_lib PUBLIC
    thecore
    Database::Client
)
