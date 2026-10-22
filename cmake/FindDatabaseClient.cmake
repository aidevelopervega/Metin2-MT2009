# ==============================================================================
# Metin2 MT2009 - Database Client Discovery (MariaDB 12.3 LTS Exclusive)
# ==============================================================================

if(TARGET Database::Client)
    return()
endif()

if(WIN32)
    set(DB_CLIENT_INCLUDE_DIRS
        "${CMAKE_SOURCE_DIR}/extern/server/include/mysql"
    )
    foreach(_opt_inc
        "${CMAKE_SOURCE_DIR}/scratch/isolated_db/mariadb-12.3.1-winx64/include"
        "${CMAKE_SOURCE_DIR}/scratch/isolated_db/mariadb-12.3.1-winx64/include/mysql"
    )
        if(EXISTS "${_opt_inc}")
            list(APPEND DB_CLIENT_INCLUDE_DIRS "${_opt_inc}")
        endif()
    endforeach()

    find_file(DB_CLIENT_LIB
        NAMES libmariadb.a
        PATHS
            "${CMAKE_SOURCE_DIR}/extern/platform/windows-x64/mingw-ucrt-gcc16/lib"
        NO_DEFAULT_PATH
    )

    if(DB_CLIENT_LIB)
        add_library(Database::Client UNKNOWN IMPORTED)
        set_target_properties(Database::Client PROPERTIES
            IMPORTED_LOCATION "${DB_CLIENT_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${DB_CLIENT_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "ws2_32;secur32;crypt32;shlwapi"
        )
        set(DatabaseClient_FOUND TRUE)
        message(STATUS "[Database::Client] Windows kütüphanesi bulundu: ${DB_CLIENT_LIB}")
    else()
        message(WARNING "[Database::Client] Windows veritabanı kütüphanesi bulunamadı!")
    endif()

elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    # FreeBSD MariaDB 12.3 LTS Connector/C arama yolları (mysql.h başlığı ve libmariadb kütüphanesi)
    find_path(DB_INCLUDE_DIR
        NAMES mysql.h
        PATHS
            /usr/local/include/mysql
            /usr/local/include/mariadb
            /usr/local/include
        NO_DEFAULT_PATH
    )
    if(NOT DB_INCLUDE_DIR)
        find_path(DB_INCLUDE_DIR
            NAMES mysql.h
            PATHS
                /usr/local/include/mysql
                /usr/local/include/mariadb
                /usr/local/include
        )
    endif()

    find_library(DB_CLIENT_LIB
        NAMES mariadb
        PATHS
            /usr/local/lib/mariadb
            /usr/local/lib/mysql
            /usr/local/lib
        NO_DEFAULT_PATH
    )
    if(NOT DB_CLIENT_LIB)
        find_library(DB_CLIENT_LIB
            NAMES mariadb
            PATHS
                /usr/local/lib/mariadb
                /usr/local/lib/mysql
                /usr/local/lib
        )
    endif()

    if(DB_INCLUDE_DIR AND DB_CLIENT_LIB)
        file(READ "${DB_INCLUDE_DIR}/mysql.h" _db_header_content)
        string(FIND "${_db_header_content}" "MARIADB_BASE_VERSION" _is_mariadb_header)
        if(EXISTS "${DB_INCLUDE_DIR}/mariadb_version.h" OR NOT _is_mariadb_header EQUAL -1)
            set(DB_HEADER_FLAVOR "MARIADB")
        else()
            set(DB_HEADER_FLAVOR "UNKNOWN")
        endif()

        get_filename_component(DB_CLIENT_LIB_REAL "${DB_CLIENT_LIB}" REALPATH)
        string(FIND "${DB_CLIENT_LIB_REAL}" "mariadb" _is_mariadb_lib)
        if(NOT _is_mariadb_lib EQUAL -1)
            set(DB_LIB_FLAVOR "MARIADB")
        else()
            set(DB_LIB_FLAVOR "UNKNOWN")
        endif()

        if(NOT "${DB_HEADER_FLAVOR}" STREQUAL "MARIADB" OR NOT "${DB_LIB_FLAVOR}" STREQUAL "MARIADB")
            message(FATAL_ERROR "[Database::Client] FATAL ABI MISMATCH: Header '${DB_INCLUDE_DIR}/mysql.h' (${DB_HEADER_FLAVOR}) and library '${DB_CLIENT_LIB}' -> '${DB_CLIENT_LIB_REAL}' (${DB_LIB_FLAVOR}) must both be MariaDB Connector/C!")
        endif()

        get_filename_component(DB_INCLUDE_PARENT "${DB_INCLUDE_DIR}" DIRECTORY)
        set(DB_CLIENT_INCLUDE_DIRS "${DB_INCLUDE_DIR}")
        if(DB_INCLUDE_PARENT AND NOT "${DB_INCLUDE_PARENT}" STREQUAL "${DB_INCLUDE_DIR}")
            list(APPEND DB_CLIENT_INCLUDE_DIRS "${DB_INCLUDE_PARENT}")
        endif()
        list(APPEND DB_CLIENT_INCLUDE_DIRS "/usr/local/include")
        list(REMOVE_DUPLICATES DB_CLIENT_INCLUDE_DIRS)

        add_library(Database::Client UNKNOWN IMPORTED)
        set_target_properties(Database::Client PROPERTIES
            IMPORTED_LOCATION "${DB_CLIENT_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${DB_CLIENT_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "z"
        )
        
        # rpath ekle
        get_filename_component(DB_LIB_DIR "${DB_CLIENT_LIB}" DIRECTORY)
        set_target_properties(Database::Client PROPERTIES
            INTERFACE_LINK_OPTIONS "-Wl,-rpath=${DB_LIB_DIR}"
        )
        set(DatabaseClient_FOUND TRUE)
        message(STATUS "[Database::Client] FreeBSD kütüphanesi doğrulandı (${DB_HEADER_FLAVOR}): ${DB_CLIENT_LIB} (Başlık: ${DB_CLIENT_INCLUDE_DIRS})")
    else()
        message(FATAL_ERROR "[Database::Client] FreeBSD üzerinde MySQL veya MariaDB kütüphanesi bulunamadı!")
    endif()
endif()
