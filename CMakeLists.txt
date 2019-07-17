# copyright defined in LICENSE.txt

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14)
include(ExternalProject)

cmake_minimum_required (VERSION 3.10)
project(history-tools VERSION 0.1 LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules")

set(default_build_type "Release")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
set(Boost_USE_STATIC_LIBS ON)

add_definitions(-DBOOST_ASIO_DISABLE_STD_EXPERIMENTAL_STRING_VIEW)

find_package(eosio.cdt)
find_package(PostgreSQL COMPONENTS Libraries)
find_library(LMDB_LIB NAMES lmdb)
find_package(Boost 1.66 REQUIRED COMPONENTS date_time filesystem chrono system iostreams program_options unit_test_framework)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JS mozjs-64)

if (PostgreSQL_INCLUDE_DIR)
  set(SKIP_PQXX_SHARED ON)
  set(SKIP_BUILD_TEST ON)
  add_subdirectory(external/libpqxx)
  set(PQXX_LIBRARIES pqxx_static)
else()
  set(PQXX_LIBRARIES "")
endif()

if (LMDB_LIB)
    set(FOUND_LMDB TRUE)
else()
    set(LMDB_LIB "")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
      message(FATAL_ERROR "GCC version must be at least 8.0.")
   endif()
   if("${CMAKE_GENERATOR}" STREQUAL "Ninja")
      add_compile_options(-fdiagnostics-color=always)
   endif()
endif()

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

# execute_process(COMMAND git submodule status --recursive
#                 COMMAND grep -c "^[+\-]"
#                 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
#                 OUTPUT_VARIABLE submodule_status
#                 OUTPUT_STRIP_TRAILING_WHITESPACE)
# if(submodule_status GREATER 0)
#    message(FATAL_ERROR "git submodules are not up to date.
# Please run the command 'git submodule update --init --recursive'.")
# endif()

include(SetupTargetMacros)
include(GNUInstallDirs)
include(VersionMacros)

set(WITH_TESTS OFF)

add_subdirectory(external/appbase)
add_subdirectory(external/fc)
add_subdirectory(external/rocksdb)

set(FOUND_ROCKSDB ON)
set(ROCKSDB_LIB rocksdb)
set(ROCKSDB_INCLUDE_DIR external/rocksdb/include)

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
            ${ROCKSDB_INCLUDE_DIR}
    )
    target_link_libraries(${APP} appbase fc ${Boost_LIBRARIES} ${LIBS} -lpthread)
    target_compile_options(${APP} PUBLIC -DAPP_NAME="${APP}" ${JS_CFLAGS_OTHER} ${FLAGS})

    string(TOLOWER ${CMAKE_BUILD_TYPE} LOWERCASE_CMAKE_BUILD_TYPE)
    if (LOWERCASE_CMAKE_BUILD_TYPE STREQUAL "debug")
        target_compile_options(${APP} PRIVATE -D DEBUG)
    endif (LOWERCASE_CMAKE_BUILD_TYPE STREQUAL "debug")

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${APP} PRIVATE -Wall -Wextra -Wno-unused-parameter -fcolor-diagnostics)
    endif()
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(${APP} PRIVATE -fdiagnostics-color=auto)
    endif()
endfunction(add_app)

add_app(history-tools "-DDEFAULT_PLUGINS=" "${PQXX_LIBRARIES};${JS_LIBRARIES};${LMDB_LIB};${ROCKSDB_LIB}")

message(STATUS "----------------------------------------------------")
message(STATUS "Checked libraries:")

function(show_found desc found missing_msg)
    set(extra_args ${ARGN})
    list(LENGTH extra_args num_extra_args)
    if(${num_extra_args} GREATER 0)
        list(GET extra_args 0 version)
    endif()
    if (found)
        message(STATUS ${desc} " found ${version}")
    else()
        message(STATUS ${desc} " ${missing_msg}")
    endif()
endfunction(show_found)

show_found("    pq:         " "${PostgreSQL_INCLUDE_DIR}" "not found; will not build pg plugins")
show_found("    lmdb:       " "${FOUND_LMDB}" "not found; will not build lmdb plugins")
show_found("    mozjs:      " "${JS_FOUND}" "not found; will not build wasm plugins")

message(STATUS "Enabled plugins:")

if (PostgreSQL_INCLUDE_DIR)
    message(STATUS "    fill_pg_plugin")
    add_app(fill-pg "-DDEFAULT_PLUGINS=fill_pg_plugin;-DINCLUDE_FILL_PG_PLUGIN" "${PQXX_LIBRARIES}")
    target_sources(history-tools PRIVATE src/fill_plugin.cpp src/pg_plugin.cpp src/fill_pg_plugin.cpp)
    target_sources(fill-pg PRIVATE src/fill_plugin.cpp src/pg_plugin.cpp src/fill_pg_plugin.cpp)
    if (JS_FOUND)
        message(STATUS "    wasm_ql_pg_plugin")
        add_app(wasm-ql-pg "-DDEFAULT_PLUGINS=wasm_ql_pg_plugin;-DINCLUDE_WASM_QL_PG_PLUGIN" "${PQXX_LIBRARIES};${JS_LIBRARIES}")
        target_sources(history-tools PRIVATE src/pg_plugin.cpp src/query_config_plugin.cpp src/wasm_ql_pg_plugin.cpp)
        target_sources(wasm-ql-pg PRIVATE src/pg_plugin.cpp src/query_config_plugin.cpp src/wasm_ql_pg_plugin.cpp src/wasm_interface.cpp src/wasm_ql_plugin.cpp)
    endif ()
endif ()

if (FOUND_LMDB)
    message(STATUS "    fill_lmdb_plugin")
    add_app(fill-lmdb "-DDEFAULT_PLUGINS=fill_lmdb_plugin;-DINCLUDE_FILL_LMDB_PLUGIN" "${LMDB_LIB}")
    target_sources(history-tools PRIVATE src/query_config_plugin.cpp src/lmdb_plugin.cpp src/fill_plugin.cpp src/fill_lmdb_plugin.cpp)
    target_sources(fill-lmdb PRIVATE src/query_config_plugin.cpp src/lmdb_plugin.cpp src/fill_plugin.cpp src/fill_lmdb_plugin.cpp)
    if (JS_FOUND)
        message(STATUS "    wasm_ql_lmdb_plugin")
        add_app(wasm-ql-lmdb "-DDEFAULT_PLUGINS=wasm_ql_lmdb_plugin;-DINCLUDE_WASM_QL_LMDB_PLUGIN" "${LMDB_LIB};${JS_LIBRARIES}")
        add_app(combo-lmdb "-DDEFAULT_PLUGINS=fill_lmdb_plugin,wasm_ql_lmdb_plugin;-DINCLUDE_FILL_LMDB_PLUGIN;-DINCLUDE_WASM_QL_LMDB_PLUGIN" "${LMDB_LIB};${JS_LIBRARIES}")
        target_sources(history-tools PRIVATE src/query_config_plugin.cpp src/lmdb_plugin.cpp src/wasm_ql_lmdb_plugin.cpp)
        target_sources(wasm-ql-lmdb PRIVATE src/query_config_plugin.cpp src/lmdb_plugin.cpp src/wasm_ql_lmdb_plugin.cpp src/wasm_interface.cpp src/wasm_ql_plugin.cpp)
        target_sources(combo-lmdb PRIVATE src/query_config_plugin.cpp src/lmdb_plugin.cpp src/wasm_ql_lmdb_plugin.cpp src/fill_plugin.cpp src/fill_lmdb_plugin.cpp src/wasm_interface.cpp src/wasm_ql_plugin.cpp)
    endif ()
endif()

if (FOUND_ROCKSDB)
    message(STATUS "    fill_rocksdb_plugin")
    add_app(fill-rocksdb "-DDEFAULT_PLUGINS=fill_rocksdb_plugin;-DINCLUDE_FILL_ROCKSDB_PLUGIN" "${ROCKSDB_LIB}")
    target_sources(history-tools PRIVATE src/query_config_plugin.cpp src/rocksdb_plugin.cpp src/fill_plugin.cpp src/fill_rocksdb_plugin.cpp)
    target_sources(fill-rocksdb PRIVATE src/query_config_plugin.cpp src/rocksdb_plugin.cpp src/fill_plugin.cpp src/fill_rocksdb_plugin.cpp)
    if (JS_FOUND)
        message(STATUS "    wasm_ql_rocksdb_plugin")
        add_app(wasm-ql-rocksdb "-DDEFAULT_PLUGINS=wasm_ql_rocksdb_plugin;-DINCLUDE_WASM_QL_ROCKSDB_PLUGIN" "${ROCKSDB_LIB};${JS_LIBRARIES}")
        add_app(combo-rocksdb "-DDEFAULT_PLUGINS=fill_rocksdb_plugin,wasm_ql_rocksdb_plugin;-DINCLUDE_FILL_ROCKSDB_PLUGIN;-DINCLUDE_WASM_QL_ROCKSDB_PLUGIN" "${ROCKSDB_LIB};${JS_LIBRARIES}")
        target_sources(history-tools PRIVATE src/query_config_plugin.cpp src/rocksdb_plugin.cpp src/wasm_ql_rocksdb_plugin.cpp)
        target_sources(wasm-ql-rocksdb PRIVATE src/query_config_plugin.cpp src/rocksdb_plugin.cpp src/wasm_ql_rocksdb_plugin.cpp src/wasm_interface.cpp src/wasm_ql_plugin.cpp)
        target_sources(combo-rocksdb PRIVATE src/query_config_plugin.cpp src/rocksdb_plugin.cpp src/wasm_ql_rocksdb_plugin.cpp src/fill_plugin.cpp src/fill_rocksdb_plugin.cpp src/wasm_interface.cpp src/wasm_ql_plugin.cpp)
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

if (NOT PostgreSQL_INCLUDE_DIR AND NOT FOUND_LMDB AND NOT FOUND_ROCKSDB)
    message(WARNING "No database libraries found. Only building WASM files.")
endif()

if (eosio.cdt_FOUND)
    ExternalProject_Add(
        wasms_project
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/wasms
        BINARY_DIR ${CMAKE_BINARY_DIR}/wasms
        CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${EOSIO_CDT_ROOT}/lib/cmake/eosio.cdt/EosioWasmToolchain.cmake
        UPDATE_COMMAND ""
        PATCH_COMMAND ""
        TEST_COMMAND ""
        INSTALL_COMMAND ""
        BUILD_ALWAYS 1
    )
endif()
