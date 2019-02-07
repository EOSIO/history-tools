# copyright defined in LICENSE.txt

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14)

cmake_minimum_required (VERSION 3.11)
project(wasm-ql VERSION 0.1 LANGUAGES CXX)

set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

FIND_PACKAGE(Boost 1.58 REQUIRED COMPONENTS date_time filesystem chrono system iostreams program_options unit_test_framework)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JS REQUIRED mozjs-64)

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

add_executable(wasm-ql
    src/fill_lmdb_plugin.cpp
    src/fill_postgresql_plugin.cpp
    src/lmdb_plugin.cpp
    src/main.cpp
    src/wasm_interface.cpp
    src/wasm_ql_lmdb_plugin.cpp
    src/wasm_ql_pg_plugin.cpp
    src/wasm_ql_plugin.cpp
)

target_include_directories(wasm-ql
    PRIVATE
        external/abieos/src
        external/abieos/external/date/include
        external/abieos/external/rapidjson/include
        external/appbase/include
        external/fc/include
        ${Boost_INCLUDE_DIR}
        ${JS_INCLUDE_DIRS}
)
target_link_libraries(wasm-ql appbase fc Boost::date_time Boost::filesystem Boost::system Boost::iostreams Boost::program_options pqxx pq ${JS_LIBRARIES} lmdb -lpthread)
target_compile_options(wasm-ql PUBLIC ${JS_CFLAGS_OTHER})

string(TOLOWER ${CMAKE_BUILD_TYPE} LOWERCASE_CMAKE_BUILD_TYPE)
if (LOWERCASE_CMAKE_BUILD_TYPE STREQUAL "debug")
    target_compile_options(wasm-ql PRIVATE -D DEBUG)
endif (LOWERCASE_CMAKE_BUILD_TYPE STREQUAL "debug")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(wasm-ql PRIVATE -Wall -Wextra -Wno-unused-parameter -fcolor-diagnostics)
endif()
