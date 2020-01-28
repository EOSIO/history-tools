# copyright defined in LICENSE.txt

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14)
include(ExternalProject)

cmake_minimum_required (VERSION 3.10)
project(history-tools VERSION 0.1 LANGUAGES CXX)

cmake_policy(SET CMP0077 NEW)
set(ENABLE_INSTALL OFF cache bool "")
set(ENABLE_TOOLS OFF cache bool "")
set(ENABLE_TESTS OFF cache bool "")
add_subdirectory(external/eos-vm EXCLUDE_FROM_ALL)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules")

set(default_build_type "Release")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if (UNIX AND APPLE)
   add_compile_options(-fvisibility=hidden)
endif()

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
set(Boost_USE_STATIC_LIBS ON)

add_definitions(-DBOOST_ASIO_DISABLE_STD_EXPERIMENTAL_STRING_VIEW)

#find_package(eosio.cdt)
find_package(Boost 1.70 REQUIRED COMPONENTS date_time filesystem chrono system iostreams program_options unit_test_framework)
find_package(PkgConfig REQUIRED)

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

set(DISABLE_CMAKE_WARNING ${SKIP_SUBMODULE_CHECK})
if(NOT DEFINED SKIP_SUBMODULE_CHECK)
    execute_process(COMMAND git submodule status --recursive
                    COMMAND grep -c "^[+\-]"
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    OUTPUT_VARIABLE submodule_status
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(submodule_status GREATER 0)
    message(FATAL_ERROR "git submodules are not up to date.
Please run the command 'git submodule update --init --recursive'.")
    endif()
endif()

include(SetupTargetMacros)
include(GNUInstallDirs)
include(VersionMacros)

set(WITH_TESTS OFF)

add_compile_options(-Wno-error=shadow)

add_subdirectory(external/appbase EXCLUDE_FROM_ALL)
add_subdirectory(external/fc EXCLUDE_FROM_ALL)

set(FOUND_ROCKSDB ON)
set(ROCKSDB_LIB rocksdb)

set(APPS "")
function(add_app APP MAIN FLAGS LIBS)
    set(APPS ${APPS} ${APP} PARENT_SCOPE)
    add_executable(${APP} src/${MAIN}.cpp)

    target_include_directories(${APP}
        PRIVATE
            external/abieos/include
            external/abieos/src
            external/abieos/external/date/include
            external/abieos/external/outcome/single-header
            external/abieos/external/rapidjson/include
            external/appbase/include
            external/chain-kv/include
            external/eos-vm/include
            external/eos-vm/external/softfloat/source/include
            external/fc/include
            ${Boost_INCLUDE_DIR}
            ${JS_INCLUDE_DIRS}
            ${ROCKSDB_INCLUDE_DIR}
    )
    target_link_libraries(${APP} appbase fc eos-vm Boost::date_time Boost::filesystem Boost::chrono Boost::system Boost::iostreams Boost::program_options Boost::unit_test_framework ${LIBS} -lpthread)

    if(APPLE)
    else()
        target_link_libraries(${APP} -latomic)
    endif()
    target_compile_options(${APP} PUBLIC -DAPP_NAME="${APP}" ${FLAGS})

    string(TOLOWER ${CMAKE_BUILD_TYPE} LOWERCASE_CMAKE_BUILD_TYPE)
    if (LOWERCASE_CMAKE_BUILD_TYPE STREQUAL "debug")
        target_compile_options(${APP} PRIVATE -D DEBUG)
    endif (LOWERCASE_CMAKE_BUILD_TYPE STREQUAL "debug")

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${APP} PRIVATE -Wall -Wextra -Wno-unused-parameter -fcolor-diagnostics -Wno-sign-compare -Wno-unused-variable -Wno-macro-redefined)
    endif()
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(${APP} PRIVATE -fdiagnostics-color=auto)
    endif()
endfunction(add_app)

# add_app(history-tools main "-DDEFAULT_PLUGINS=" "${ROCKSDB_LIB}")
# add_app(boing boing "" "${ROCKSDB_LIB}")

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

show_found("    rocksdb:    " "${FOUND_ROCKSDB}" "not found; will not build rocksdb plugins")

message(STATUS "Enabled plugins:")

if (FOUND_ROCKSDB)
    message(STATUS "    fill_rocksdb_plugin")
    add_app(fill-rocksdb main "-DDEFAULT_PLUGINS=fill_rocksdb_plugin;-DINCLUDE_FILL_ROCKSDB_PLUGIN" "${ROCKSDB_LIB}")
    #target_sources(history-tools PRIVATE src/query_config_plugin.cpp src/rocksdb_plugin.cpp src/fill_plugin.cpp src/fill_rocksdb_plugin.cpp)
    target_sources(fill-rocksdb PRIVATE src/rocksdb_plugin.cpp src/fill_plugin.cpp src/fill_rocksdb_plugin.cpp)
    message(STATUS "    wasm_ql_plugin")
    add_app(wasm-ql main "-DDEFAULT_PLUGINS=wasm_ql_plugin;-DINCLUDE_WASM_QL_PLUGIN" "${ROCKSDB_LIB}")
    add_app(combo-rocksdb main "-DDEFAULT_PLUGINS=fill_rocksdb_plugin,wasm_ql_plugin;-DINCLUDE_FILL_ROCKSDB_PLUGIN;-DINCLUDE_WASM_QL_PLUGIN" "${ROCKSDB_LIB}")
    #target_sources(history-tools PRIVATE src/query_config_plugin.cpp src/rocksdb_plugin.cpp)
    target_sources(wasm-ql PRIVATE src/rocksdb_plugin.cpp src/wasm_ql_plugin.cpp src/wasm_ql_http.cpp src/wasm_ql.cpp)
    target_sources(combo-rocksdb PRIVATE src/rocksdb_plugin.cpp src/fill_plugin.cpp src/fill_rocksdb_plugin.cpp src/wasm_ql_plugin.cpp src/wasm_ql_http.cpp src/wasm_ql.cpp)
    message(STATUS "    wasm_ql_plugin")
    #target_sources(history-tools PRIVATE src/rocksdb_plugin.cpp src/wasm_plugin.cpp src/wasm_dispatcher.cpp src/wasm_ql_plugin.cpp src/wasm_ql_http.cpp src/wasm_ql.cpp)
endif()

message(STATUS "Enabled apps:")
foreach(APP ${APPS})
    message(STATUS "    ${APP}")
endforeach()

message(STATUS "----------------------------------------------------")

if (NOT FOUND_ROCKSDB)
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
