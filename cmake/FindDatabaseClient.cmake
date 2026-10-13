# ==============================================================================
# Metin2 MT2009 - Database Client Discovery (MySQL 8.4 / MariaDB 11.4 / MariaDB 12.3)
# ==============================================================================

if(TARGET Database::Client)
    return()
endif()

if(WIN32)
    set(DB_CLIENT_INCLUDE_DIRS
        "${CMAKE_SOURCE_DIR}/extern/server/include/mysql"
        "${CMAKE_SOURCE_DIR}/scratch/isolated_db/mariadb-11.4.13-winx64/include"
        "${CMAKE_SOURCE_DIR}/scratch/isolated_db/mariadb-11.4.13-winx64/include/mysql"
    )

    find_file(DB_CLIENT_LIB
        NAMES libmariadb.a libmysqlclient.a
        PATHS
            "${CMAKE_SOURCE_DIR}/extern/platform/windows-x64/mingw-ucrt-gcc16/lib"
            "${CMAKE_SOURCE_DIR}/scratch/isolated_db/mariadb-11.4.13-winx64/lib"
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
    # FreeBSD MariaDB ve MySQL arama yolları
    find_path(DB_INCLUDE_DIR
        NAMES mysql.h
        PATHS
            /usr/local/include/mysql
            /usr/local/include/mariadb
            /usr/local/include
    )

    find_library(DB_CLIENT_LIB
        NAMES mariadb mysqlclient
        PATHS
            /usr/local/lib/mariadb
            /usr/local/lib/mysql
            /usr/local/lib
    )

    if(DB_INCLUDE_DIR AND DB_CLIENT_LIB)
        add_library(Database::Client UNKNOWN IMPORTED)
        set_target_properties(Database::Client PROPERTIES
            IMPORTED_LOCATION "${DB_CLIENT_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${DB_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "z"
        )
        
        # rpath ekle
        get_filename_component(DB_LIB_DIR "${DB_CLIENT_LIB}" DIRECTORY)
        set_target_properties(Database::Client PROPERTIES
            INTERFACE_LINK_OPTIONS "-Wl,-rpath=${DB_LIB_DIR}"
        )
        set(DatabaseClient_FOUND TRUE)
        message(STATUS "[Database::Client] FreeBSD kütüphanesi bulundu: ${DB_CLIENT_LIB} (Başlık: ${DB_INCLUDE_DIR})")
    else()
        message(FATAL_ERROR "[Database::Client] FreeBSD üzerinde MySQL veya MariaDB kütüphanesi bulunamadı!")
    endif()
endif()
