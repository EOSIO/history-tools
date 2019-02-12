# copyright defined in LICENSE.txt

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14)

cmake_minimum_required (VERSION 3.11)
project(wasm-ql VERSION 0.1 LANGUAGES CXX)

set(default_build_type "Release")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

find_library(PQ_LIB NAMES pq)
find_library(PQXX_LIB NAMES pqxx)
find_library(LMDB_LIB NAMES lmdb)
find_package(Boost 1.58 REQUIRED COMPONENTS date_time filesystem chrono system iostreams program_options unit_test_framework)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JS mozjs-64)

if (PQ_LIB AND PQXX_LIB)
    set(FOUND_PQ TRUE)
else()
    set(PQ_LIB "")
    set(PQXX_LIB "")
endif()

if (LMDB_LIB)
    set(FOUND_LMDB TRUE)
else()
    set(LMDB_LIB "")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/fc/CMakeModules")

if ("${OPENSSL_ROOT_DIR}" STREQUAL "")
   if (NOT "$ENV{OPENSSL_ROOT_DIR}" STREQUAL "")
      set(OPENSSL_ROOT_DIR $ENV{OPENSSL_ROOT_DIR})
      set(OPENSSL_INCLUDE_DIR ${OPENSSL_ROOT_DIR}/include)
   elseif (APPLE)
      set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
      set(OPENSSL_INCLUDE_DIR "/usr/local/opt/openssl/include")
   elseif(UNIX AND NOT APPLE)
      set(OPENSSL_ROOT_DIR "/usr/include/openssl")
      set(OPENSSL_INCLUDE_DIR "/usr/include/openssl/include")
   else()
      message(FATAL_ERROR "openssl not found and don't know where to look, please specify OPENSSL_ROOT_DIR")
   endif()
endif()

set(SKIP_FC_TESTS ON)

include(SetupTargetMacros)
include(GNUInstallDirs)
include(VersionMacros)

add_subdirectory(external/appbase)
add_subdirectory(external/fc)

set(APPS "")
function(add_app APP FLAGS LIBS)
    set(APPS ${APPS} ${APP} PARENT_SCOPE)
    add_executable(${APP} src/main.cpp)

    target_include_directories(${APP}
        PRIVATE
            external/abieos/src
            external/abieos/external/date/include
            external/abieos/external/rapidjson/include
            external/appbase/include
            external/fc/include
            ${Boost_INCLUDE_DIR}
            ${JS_INCLUDE_DIRS}
    )
    target_link_libraries(${APP} appbase fc Boost::date_time Boost::filesystem Boost::system Boost::iostreams Boost::program_options ${LIBS} -lpthread)
    target_compile_options(${APP} PUBLIC -DAPP_NAME="${APP}" ${JS_CFLAGS_OTHER} ${FLAGS})

    string(TOLOWER ${CMAKE_BUILD_TYPE} LOWERCASE_CMAKE_BUILD_TYPE)
    if (LOWERCASE_CMAKE_BUILD_TYPE STREQUAL "debug")
        target_compile_options(${APP} PRIVATE -D DEBUG)
    endif (LOWERCASE_CMAKE_BUILD_TYPE STREQUAL "debug")

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${APP} PRIVATE -Wall -Wextra -Wno-unused-parameter -fcolor-diagnostics)
    endif()
endfunction(add_app)

add_app(history-tools "-DDEFAULT_PLUGINS=" "${PQXX_LIB};${PQ_LIB};${JS_LIBRARIES};${LMDB_LIB}")

message(STATUS "----------------------------------------------------")
message(STATUS "Checked libraries:")

function(show_found desc found missing_msg)
    if (found)
        message(STATUS ${desc} " found")
    else()
        message(STATUS ${desc} " ${missing_msg}")
    endif()
endfunction(show_found)

show_found("    pq and pqxx:" "${FOUND_PQ}" "not found; will not build pg plugins")
show_found("    lmdb:       " "${FOUND_LMDB}" "not found; will not build lmdb plugins")
show_found("    mozjs:      " "${JS_FOUND}" "not found; will not build wasm plugins")

message(STATUS "Enabled plugins:")

if (FOUND_PQ)
    message(STATUS "    fill_pg_plugin")
    add_app(fill-pg "-DDEFAULT_PLUGINS=fill_pg_plugin;-DINCLUDE_FILL_PG_PLUGIN" "${PQXX_LIB};${PQ_LIB}")
    target_sources(history-tools PRIVATE src/fill_pg_plugin.cpp)
    target_sources(fill-pg PRIVATE src/fill_pg_plugin.cpp)
    if (JS_FOUND)
        message(STATUS "    wasm_ql_pg_plugin")
        add_app(wasm-ql-pg "-DDEFAULT_PLUGINS=wasm_ql_pg_plugin;-DINCLUDE_WASM_QL_PG_PLUGIN" "${PQXX_LIB};${PQ_LIB};${JS_LIBRARIES}")
        target_sources(history-tools PRIVATE src/wasm_ql_pg_plugin.cpp)
        target_sources(wasm-ql-pg PRIVATE src/wasm_ql_pg_plugin.cpp src/wasm_interface.cpp src/wasm_ql_plugin.cpp)
    endif ()
endif ()

if (FOUND_LMDB)
    message(STATUS "    fill_lmdb_plugin")
    add_app(fill-lmdb "-DDEFAULT_PLUGINS=fill_lmdb_plugin;-DINCLUDE_FILL_LMDB_PLUGIN" "${LMDB_LIB}")
    target_sources(history-tools PRIVATE src/lmdb_plugin.cpp src/fill_lmdb_plugin.cpp)
    target_sources(fill-lmdb PRIVATE src/lmdb_plugin.cpp src/fill_lmdb_plugin.cpp)
    if (JS_FOUND)
        message(STATUS "    wasm_ql_lmdb_plugin")
        add_app(wasm-ql-lmdb "-DDEFAULT_PLUGINS=wasm_ql_lmdb_plugin;-DINCLUDE_WASM_QL_LMDB_PLUGIN" "${LMDB_LIB};${JS_LIBRARIES}")
        add_app(combo-lmdb "-DDEFAULT_PLUGINS=fill_lmdb_plugin,wasm_ql_lmdb_plugin;-DINCLUDE_FILL_LMDB_PLUGIN;-DINCLUDE_WASM_QL_LMDB_PLUGIN" "${LMDB_LIB};${JS_LIBRARIES}")
        target_sources(history-tools PRIVATE src/lmdb_plugin.cpp src/wasm_ql_lmdb_plugin.cpp)
        target_sources(wasm-ql-lmdb PRIVATE src/lmdb_plugin.cpp src/wasm_ql_lmdb_plugin.cpp src/wasm_interface.cpp src/wasm_ql_plugin.cpp)
        target_sources(combo-lmdb PRIVATE src/lmdb_plugin.cpp src/wasm_ql_lmdb_plugin.cpp src/fill_lmdb_plugin.cpp src/wasm_interface.cpp src/wasm_ql_plugin.cpp)
    endif ()
endif()

if (JS_FOUND)
    message(STATUS "    wasm_ql_plugin")
    target_sources(history-tools PRIVATE src/wasm_interface.cpp src/wasm_ql_plugin.cpp)
endif()

message(STATUS "Enabled apps:")
foreach(APP ${APPS})
    message(STATUS "    ${APP}")
endforeach()

message(STATUS "----------------------------------------------------")

if (NOT FOUND_PQ AND NOT FOUND_LMDB)
    message(FATAL_ERROR "No database libraries found")
endif()
